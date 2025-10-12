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
    void Expand_ByControlPoint(FbxMesh* mesh, FBXModel& model, const std::vector<int> &materialMap)
    {
        int vertexCount = mesh->GetControlPointsCount();
        FbxVector4* controlPoints = mesh->GetControlPoints();

        std::vector<glm::vec3> positions;
        positions.reserve(vertexCount);
        for (int i = 0; i < vertexCount; ++i) {
            positions.push_back({static_cast<float>(controlPoints[i][0]), static_cast<float>(controlPoints[i][1]), static_cast<float>(controlPoints[i][2])});
        }

        int polygonCount = mesh->GetPolygonCount();
        std::vector<uint32_t> indices;
        indices.reserve(polygonCount * 3);
        for (int p = 0; p < polygonCount; ++p)
        {
            int polySize = mesh->GetPolygonSize(p);
            for (int j = 0; j < polySize && j < 3; ++j)
            {
                int idx = mesh->GetPolygonVertex(p, j);
                indices.push_back(static_cast<uint32_t>(idx));
            }
        }

        // Normals if mapping is by control point
        std::vector<glm::vec3> normals;
        FbxGeometryElementNormal* normalElement = mesh->GetElementNormal();
        if (normalElement && normalElement->GetMappingMode() == FbxLayerElement::eByControlPoint)
        {
            int normalCount = normalElement->GetDirectArray().GetCount();
            normals.reserve(normalCount);
            for (int i = 0; i < normalCount && i < vertexCount; ++i) {
                FbxVector4 n = normalElement->GetDirectArray().GetAt(i);
                normals.push_back({static_cast<float>(n[0]), static_cast<float>(n[1]), static_cast<float>(n[2])});
            }
        }

        // UVs per control point
        std::vector<std::vector<glm::vec2>> allUVs;
        const int uvElementCount = mesh->GetElementUVCount();
        for (int uvIdx = 0; uvIdx < uvElementCount; ++uvIdx)
        {
            FbxGeometryElementUV* uvElement = mesh->GetElementUV(uvIdx);
            if (!uvElement) continue;
            if (uvElement->GetMappingMode() != FbxLayerElement::eByControlPoint) continue;
            std::vector<glm::vec2> uvs;
            int uvCount = uvElement->GetDirectArray().GetCount();
            uvs.reserve(uvCount);
            for (int i = 0; i < uvCount && i < vertexCount; ++i) {
                FbxVector2 uv = uvElement->GetDirectArray().GetAt(i);
                uvs.push_back({static_cast<float>(uv[0]), static_cast<float>(uv[1])});
            }
            allUVs.push_back(std::move(uvs));
        }

        pure::Geometry geometry;
        geometry.primitiveType = PrimitiveType::Triangles;
        geometry.positions = positions;

        GeometryAttribute posAttr;
        posAttr.name = "POSITION";
        posAttr.count = positions.size();
        posAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
        posAttr.data.resize(positions.size() * 3 * sizeof(float));
        memcpy(posAttr.data.data(), positions.data(), positions.size() * 3 * sizeof(float));
        geometry.attributes.push_back(posAttr);

        if (!normals.empty()) {
            GeometryAttribute normAttr;
            normAttr.name = "NORMAL";
            normAttr.count = normals.size();
            normAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
            normAttr.data.resize(normals.size() * 3 * sizeof(float));
            memcpy(normAttr.data.data(), normals.data(), normals.size() * 3 * sizeof(float));
            geometry.attributes.push_back(normAttr);
        }

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

        // indices
        const size_t vertexCountSize = positions.size();
        pure::GeometryIndicesMeta indicesMeta;
        indicesMeta.count = indices.size();
        if (g_allow_u8_indices && vertexCountSize <= static_cast<size_t>(std::numeric_limits<uint8_t>::max()))
        {
            std::vector<uint8_t> idx8;
            idx8.reserve(indices.size());
            for (uint32_t v : indices) idx8.push_back(static_cast<uint8_t>(v));
            geometry.indicesData = std::vector<std::byte>(idx8.size() * sizeof(uint8_t));
            memcpy(geometry.indicesData->data(), idx8.data(), geometry.indicesData->size());
            indicesMeta.indexType = IndexType::U8;
        }
        else if (vertexCountSize <= static_cast<size_t>(std::numeric_limits<uint16_t>::max()))
        {
            std::vector<uint16_t> idx16;
            idx16.reserve(indices.size());
            for (uint32_t v : indices) idx16.push_back(static_cast<uint16_t>(v));
            geometry.indicesData = std::vector<std::byte>(idx16.size() * sizeof(uint16_t));
            memcpy(geometry.indicesData->data(), idx16.data(), geometry.indicesData->size());
            indicesMeta.indexType = IndexType::U16;
        }
        else
        {
            geometry.indicesData = std::vector<std::byte>(indices.size() * sizeof(uint32_t));
            memcpy(geometry.indicesData->data(), indices.data(), geometry.indicesData->size());
            indicesMeta.indexType = IndexType::U32;
        }
        geometry.indices = indicesMeta;

        if (!positions.empty()) {
            glm::vec3 min = positions[0];
            glm::vec3 max = positions[0];
            for (const auto& p : positions) { min = glm::min(min, p); max = glm::max(max, p); }
            geometry.bounding_volume.aabb.min = min;
            geometry.bounding_volume.aabb.max = max;
        }

        int32_t geomIndex = static_cast<int32_t>(model.geometry.size());
        model.geometry.push_back(std::move(geometry));

        pure::Primitive prim;
        prim.geometry = geomIndex;
        // no material info per control-point strategy; if single material on node, map it
        if (!materialMap.empty()) prim.material = materialMap[0] >= 0 ? std::optional<int32_t>(static_cast<int32_t>(materialMap[0])) : std::nullopt;
        int32_t primIndex = static_cast<int32_t>(model.primitives.size());
        model.primitives.push_back(prim);

        pure::Mesh pm;
        pm.name = mesh->GetName() ? mesh->GetName() : std::string();
        pm.primitives.push_back(primIndex);
        if (!positions.empty()) {
            glm::vec3 mn = positions[0]; glm::vec3 mx = positions[0];
            for (const auto &p : positions) { mn = glm::min(mn, p); mx = glm::max(mx, p); }
            pm.bounding_volume.aabb.min = mn; pm.bounding_volume.aabb.max = mx;
        }
        model.meshes.push_back(std::move(pm));
    }
}
