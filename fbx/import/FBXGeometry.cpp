#include "fbx/import/FBXGeometry.h"
#include "fbx/import/FBXImporter.h"

#include <vector>
#include <glm/glm.hpp>
#include <limits>
#include "pure/Model.h"
#include "pure/Geometry.h"
#include "pure/Primitive.h"
#include "common/GeometryAttribute.h"
#include "common/PrimitiveType.h"
#include "common/IndexType.h"
#include "common/VkFormat.h"

namespace fbx
{
    void ProcessMesh(FbxMesh* mesh, FBXModel& model)
    {
        // Triangulate the mesh
        FbxGeometryConverter converter(mesh->GetFbxManager());
        converter.Triangulate(mesh, true);

        int vertexCount = mesh->GetControlPointsCount();
        FbxVector4* controlPoints = mesh->GetControlPoints();

        // Positions
        std::vector<glm::vec3> positions;
        for (int i = 0; i < vertexCount; ++i) {
            positions.push_back({static_cast<float>(controlPoints[i][0]), static_cast<float>(controlPoints[i][1]), static_cast<float>(controlPoints[i][2])});
        }

        // Indices
        int polygonCount = mesh->GetPolygonCount();
        std::vector<uint32_t> indices;
        for (int i = 0; i < polygonCount; ++i) {
            for (int j = 0; j < 3; ++j) {
                int index = mesh->GetPolygonVertex(i, j);
                indices.push_back(static_cast<uint32_t>(index));
            }
        }

        // Normals
        std::vector<glm::vec3> normals;
        FbxGeometryElementNormal* normalElement = mesh->GetElementNormal();
        if (normalElement) {
            int normalCount = normalElement->GetDirectArray().GetCount();
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
            FbxGeometryElementUV* uvElement = mesh->GetElementUV(uvIdx);
            if (!uvElement) continue;
            std::vector<glm::vec2> uvs;
            int uvCount = uvElement->GetDirectArray().GetCount();
            for (int i = 0; i < uvCount; ++i) {
                FbxVector2 uv = uvElement->GetDirectArray().GetAt(i);
                uvs.push_back({static_cast<float>(uv[0]), static_cast<float>(uv[1])});
            }
            allUVs.push_back(std::move(uvs));
        }

        // Tangents and binormals (if available)
        std::vector<glm::vec4> tangents; // store as vec4 (GLTF-style: vec4, w can be sign for bitangent)
        FbxGeometryElementTangent* tanElement = mesh->GetElementTangent();
        if (tanElement)
        {
            int tcount = tanElement->GetDirectArray().GetCount();
            for (int i = 0; i < tcount; ++i)
            {
                FbxVector4 t = tanElement->GetDirectArray().GetAt(i);
                // FBX tangent may be 3 or 4 components; fill w with 1.0 if not present
                tangents.push_back({static_cast<float>(t[0]), static_cast<float>(t[1]), static_cast<float>(t[2]), static_cast<float>(t[3])});
            }
        }

        std::vector<glm::vec3> binormals;
        FbxGeometryElementBinormal* binElement = mesh->GetElementBinormal();
        if (binElement)
        {
            int bcount = binElement->GetDirectArray().GetCount();
            for (int i = 0; i < bcount; ++i)
            {
                FbxVector4 b = binElement->GetDirectArray().GetAt(i);
                binormals.push_back({static_cast<float>(b[0]), static_cast<float>(b[1]), static_cast<float>(b[2])});
            }
        }

        // Create pure::Geometry
        pure::Geometry geometry;
        geometry.primitiveType = PrimitiveType::Triangles;
        geometry.positions = positions;

        // Positions attribute
        GeometryAttribute posAttr;
        posAttr.name = "POSITION";
        posAttr.count = positions.size();
        posAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
        // glm::vec3 may have padding; calculate byte size using float count
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
            bitAttr.name = "BINORMAL"; // keep name clear; some systems use BITANGENT
            bitAttr.count = binormals.size();
            bitAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
            bitAttr.data.resize(binormals.size() * 3 * sizeof(float));
            memcpy(bitAttr.data.data(), binormals.data(), binormals.size() * 3 * sizeof(float));
            geometry.attributes.push_back(bitAttr);
        }

        // Indices: choose U16 if possible, otherwise U32
        {
            const size_t vertexCountSize = positions.size();
            pure::GeometryIndicesMeta indicesMeta;
            indicesMeta.count = indices.size();
            if (g_allow_u8_indices && vertexCountSize <= static_cast<size_t>(std::numeric_limits<uint8_t>::max()))
            {
                // convert to uint8_t
                std::vector<uint8_t> idx8;
                idx8.reserve(indices.size());
                for (uint32_t v : indices) idx8.push_back(static_cast<uint8_t>(v));
                geometry.indicesData = std::vector<std::byte>(idx8.size() * sizeof(uint8_t));
                memcpy(geometry.indicesData->data(), idx8.data(), geometry.indicesData->size());
                indicesMeta.indexType = IndexType::U8;
            }
            else if (vertexCountSize <= static_cast<size_t>(std::numeric_limits<uint16_t>::max()))
            {
                // convert to uint16_t
                std::vector<uint16_t> idx16;
                idx16.reserve(indices.size());
                for (uint32_t v : indices) idx16.push_back(static_cast<uint16_t>(v));
                geometry.indicesData = std::vector<std::byte>(idx16.size() * sizeof(uint16_t));
                memcpy(geometry.indicesData->data(), idx16.data(), geometry.indicesData->size());
                indicesMeta.indexType = IndexType::U16;
            }
            else
            {
                // keep as uint32_t
                geometry.indicesData = std::vector<std::byte>(indices.size() * sizeof(uint32_t));
                memcpy(geometry.indicesData->data(), indices.data(), geometry.indicesData->size());
                indicesMeta.indexType = IndexType::U32;
            }
            geometry.indices = indicesMeta;
        }

        // Bounding volume
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
        size_t geomIndex = model.geometry.size();
        model.geometry.push_back(std::move(geometry));

        // Create primitive
        pure::Primitive primitive;
        primitive.geometry = static_cast<int32_t>(geomIndex);
        model.primitives.push_back(primitive);
    }
}
