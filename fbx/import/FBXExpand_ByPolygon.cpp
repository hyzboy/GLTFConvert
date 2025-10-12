#include "fbx/import/FBXGeometry.h"
#include "fbx/import/FBXImporter.h"

#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
#include <limits>
#include "pure/Model.h"
#include "pure/Geometry.h"
#include "pure/Primitive.h"
#include "common/GeometryAttribute.h"
#include "common/PrimitiveType.h"
#include "common/IndexType.h"
#include "common/VkFormat.h"

#include <fbxsdk.h>
#include <iostream>
using namespace fbxsdk;

namespace fbx
{
    void Expand_ByPolygon(FbxMesh* mesh, FBXModel& model, const std::vector<int> &materialMap)
    {
        // Mesh is expected to be triangulated by the caller (ProcessMesh). Do not triangulate here.

        int vertexCount = mesh->GetControlPointsCount();
        FbxVector4* controlPoints = mesh->GetControlPoints();

        // Positions
        std::vector<glm::vec3> positions;
        positions.reserve(vertexCount);
        for (int i = 0; i < vertexCount; ++i) {
            positions.push_back({static_cast<float>(controlPoints[i][0]), static_cast<float>(controlPoints[i][1]), static_cast<float>(controlPoints[i][2])});
        }

        // Indices per material (collect triangles per polygon material)
        int polygonCount = mesh->GetPolygonCount();
        // map material index -> list of triangle indices (flattened)
        std::unordered_map<int, std::vector<uint32_t>> indicesByMaterial;
        fbxsdk::FbxGeometryElementMaterial* matElem = mesh->GetElementMaterial();
        bool perPolygonMaterial = (matElem && matElem->GetMappingMode() == fbxsdk::FbxLayerElement::eByPolygon);
        for (int p = 0; p < polygonCount; ++p)
        {
            int matIndex = -1;
            if (perPolygonMaterial)
            {
                // If reference mode is IndexToDirect, index array gives material per polygon
                if (matElem->GetReferenceMode() == fbxsdk::FbxLayerElement::eIndexToDirect)
                {
                    matIndex = matElem->GetIndexArray().GetAt(p);
                }
                else
                {
                    // Reference mode is not IndexToDirect. Accessing the direct array is considered
                    // private in some FBX SDK versions; avoid calling GetDirectArray() here.
                    // Fallback to unknown material (-1). If needed, material mapping for this
                    // reference mode should be resolved elsewhere using node/material connections.
                    matIndex = -1;
                }
            }
            // assume triangles after triangulation
            int polySize = mesh->GetPolygonSize(p);
            for (int j = 0; j < polySize && j < 3; ++j)
            {
                int index = mesh->GetPolygonVertex(p, j);
                indicesByMaterial[matIndex].push_back(static_cast<uint32_t>(index));
            }
        }

        // Normals
        std::vector<glm::vec3> normals;
        fbxsdk::FbxGeometryElementNormal* normalElement = mesh->GetElementNormal();
        if (normalElement) {
            int normalCount = normalElement->GetDirectArray().GetCount();
            normals.reserve(normalCount);
            for (int i = 0; i < normalCount; ++i) {
                FbxVector4 n = normalElement->GetDirectArray().GetAt(i);
                normals.push_back({static_cast<float>(n[0]), static_cast<float>(n[1]), static_cast<float>(n[2])});
            }
        }
        // UVs - support multiple UV sets (TEXCOORD_0, TEXCOORD_1, ...)
        std::vector<std::vector<glm::vec2>> allUVs;
        const int uvElementCount = mesh->GetElementUVCount();
        for (int uvIdx = 0; uvIdx < uvElementCount; ++uvIdx)
        {
            fbxsdk::FbxGeometryElementUV* uvElement = mesh->GetElementUV(uvIdx);
            if (!uvElement) continue;
            std::vector<glm::vec2> uvs;
            int uvCount = uvElement->GetDirectArray().GetCount();
            uvs.reserve(uvCount);
            for (int i = 0; i < uvCount; ++i) {
                FbxVector2 uv = uvElement->GetDirectArray().GetAt(i);
                uvs.push_back({static_cast<float>(uv[0]), static_cast<float>(uv[1])});
            }
            allUVs.push_back(std::move(uvs));
        }

        // Tangents and binormals (if available)
        std::vector<glm::vec4> tangents; // store as vec4 (GLTF-style: vec4, w can be sign for bitangent)
        fbxsdk::FbxGeometryElementTangent* tanElement = mesh->GetElementTangent();
        if (tanElement)
        {
            int tcount = tanElement->GetDirectArray().GetCount();
            tangents.reserve(tcount);
            for (int i = 0; i < tcount; ++i)
            {
                FbxVector4 t = tanElement->GetDirectArray().GetAt(i);
                tangents.push_back({static_cast<float>(t[0]), static_cast<float>(t[1]), static_cast<float>(t[2]), static_cast<float>(t[3])});
            }
        }

        std::vector<glm::vec3> binormals;
        fbxsdk::FbxGeometryElementBinormal* binElement = mesh->GetElementBinormal();
        if (binElement)
        {
            int bcount = binElement->GetDirectArray().GetCount();
            binormals.reserve(bcount);
            for (int i = 0; i < bcount; ++i)
            {
                FbxVector4 b = binElement->GetDirectArray().GetAt(i);
                binormals.push_back({static_cast<float>(b[0]), static_cast<float>(b[1]), static_cast<float>(b[2])});
            }
        }

        // Create a mesh grouping multiple primitives (one per material used)
        pure::Mesh pm;
        pm.name = mesh->GetName() ? mesh->GetName() : std::string();

        // For each material group, create a Geometry + Primitive
        for (auto &kv : indicesByMaterial)
        {
            int matIndex = kv.first; // may be -1 for no-material
            auto &idxList = kv.second;

            pure::Geometry geometry;
            geometry.primitiveType = PrimitiveType::Triangles;
            geometry.positions = positions;

            // Positions attribute
            GeometryAttribute posAttr;
            posAttr.name = "POSITION";
            posAttr.count = positions.size();
            posAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
            posAttr.data.resize(positions.size() * 3 * sizeof(float));
            memcpy(posAttr.data.data(), positions.data(), positions.size() * 3 * sizeof(float));
            geometry.attributes.push_back(posAttr);

            // Normals if available
            if (!normals.empty()) {
                GeometryAttribute normAttr;
                normAttr.name = "NORMAL";
                normAttr.count = normals.size();
                normAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
                normAttr.data.resize(normals.size() * 3 * sizeof(float));
                memcpy(normAttr.data.data(), normals.data(), normals.size() * 3 * sizeof(float));
                geometry.attributes.push_back(normAttr);
            }

            // UV attributes
            for (size_t i = 0; i < allUVs.size(); ++i)
            {
                const auto &uvs = allUVs[i];
                if (uvs.empty()) continue;
                GeometryAttribute uvAttr;
                uvAttr.name = std::string("TEXCOORD_") + std::to_string(static_cast<int>(i));
                uvAttr.count = uvs.size();
                uvAttr.format = VK_FORMAT_R32G32_SFLOAT;
                uvAttr.data.resize(uvs.size() * 2 * sizeof(float));
                memcpy(uvAttr.data.data(), uvs.data(), uvs.size() * 2 * sizeof(float));
                geometry.attributes.push_back(uvAttr);
            }

            // Tangents
            if (!tangents.empty())
            {
                GeometryAttribute tanAttr;
                tanAttr.name = "TANGENT"; // glTF-style
                tanAttr.count = tangents.size();
                tanAttr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
                tanAttr.data.resize(tangents.size() * 4 * sizeof(float));
                memcpy(tanAttr.data.data(), tangents.data(), tangents.size() * 4 * sizeof(float));
                geometry.attributes.push_back(tanAttr);
            }

            // Binormals / bitangents
            if (!binormals.empty())
            {
                GeometryAttribute bitAttr;
                bitAttr.name = "BINORMAL";
                bitAttr.count = binormals.size();
                bitAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
                bitAttr.data.resize(binormals.size() * 3 * sizeof(float));
                memcpy(bitAttr.data.data(), binormals.data(), binormals.size() * 3 * sizeof(float));
                geometry.attributes.push_back(bitAttr);
            }

            // Indices: choose U8/U16/U32 per material group
            {
                const size_t vertexCountSize = positions.size();
                pure::GeometryIndicesMeta indicesMeta;
                indicesMeta.count = idxList.size();
                if (g_allow_u8_indices && vertexCountSize <= static_cast<size_t>(std::numeric_limits<uint8_t>::max()))
                {
                    std::vector<uint8_t> idx8;
                    idx8.reserve(idxList.size());
                    for (uint32_t v : idxList) idx8.push_back(static_cast<uint8_t>(v));
                    geometry.indicesData = std::vector<std::byte>(idx8.size() * sizeof(uint8_t));
                    memcpy(geometry.indicesData->data(), idx8.data(), geometry.indicesData->size());
                    indicesMeta.indexType = IndexType::U8;
                }
                else if (vertexCountSize <= static_cast<size_t>(std::numeric_limits<uint16_t>::max()))
                {
                    std::vector<uint16_t> idx16;
                    idx16.reserve(idxList.size());
                    for (uint32_t v : idxList) idx16.push_back(static_cast<uint16_t>(v));
                    geometry.indicesData = std::vector<std::byte>(idx16.size() * sizeof(uint16_t));
                    memcpy(geometry.indicesData->data(), idx16.data(), geometry.indicesData->size());
                    indicesMeta.indexType = IndexType::U16;
                }
                else
                {
                    geometry.indicesData = std::vector<std::byte>(idxList.size() * sizeof(uint32_t));
                    memcpy(geometry.indicesData->data(), idxList.data(), geometry.indicesData->size());
                    indicesMeta.indexType = IndexType::U32;
                }
                geometry.indices = indicesMeta;
            }

            // Bounding volume (same for all sub-geometries)
            if (!positions.empty()) {
                glm::vec3 min = positions[0];
                glm::vec3 max = positions[0];
                for (const auto& p : positions) {
                    min = glm::min(min, p);
                    max = glm::max(max, p);
                }
                geometry.bounding_volume.aabb.min = min;
                geometry.bounding_volume.aabb.max = max;
            }

            // Add geometry to model
            int32_t geomIndex = static_cast<int32_t>(model.geometry.size());
            model.geometry.push_back(std::move(geometry));

            // Create primitive referencing geometry and material
            pure::Primitive primitive;
            primitive.geometry = geomIndex;
            if (matIndex >= 0 && matIndex < static_cast<int>(materialMap.size())) {
                int mapped = materialMap[matIndex];
                if (mapped >= 0) primitive.material = static_cast<int32_t>(mapped);
            }
            int32_t primIndex = static_cast<int32_t>(model.primitives.size());
            model.primitives.push_back(primitive);

            // Add primitive index to mesh
            pm.primitives.push_back(primIndex);
        }

        // compute mesh bounds from geometry positions if available
        if (!pm.primitives.empty())
        {
            if (!positions.empty()) {
                glm::vec3 mn = positions[0];
                glm::vec3 mx = positions[0];
                for (const auto &p : positions) { mn = glm::min(mn, p); mx = glm::max(mx, p); }
                pm.bounding_volume.aabb.min = mn;
                pm.bounding_volume.aabb.max = mx;
            }
        }

        // Add mesh to model (one mesh per FbxMesh)
        model.meshes.push_back(std::move(pm));
    }
}
