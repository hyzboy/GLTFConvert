#ifdef USE_FBX

#include "fbx/import/FBXGeometry.h"

#include <vector>
#include <glm/glm.hpp>
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

        // UVs
        std::vector<glm::vec2> uvs;
        FbxGeometryElementUV* uvElement = mesh->GetElementUV(0);
        if (uvElement) {
            int uvCount = uvElement->GetDirectArray().GetCount();
            for (int i = 0; i < uvCount; ++i) {
                FbxVector2 uv = uvElement->GetDirectArray().GetAt(i);
                uvs.push_back({static_cast<float>(uv[0]), static_cast<float>(uv[1])});
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
        posAttr.data.resize(positions.size() * sizeof(glm::vec3));
        memcpy(posAttr.data.data(), positions.data(), posAttr.data.size());
        geometry.attributes.push_back(posAttr);

        // Normals if available
        if (!normals.empty()) {
            GeometryAttribute normAttr;
            normAttr.name = "NORMAL";
            normAttr.count = normals.size();
            normAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
            normAttr.data.resize(normals.size() * sizeof(glm::vec3));
            memcpy(normAttr.data.data(), normals.data(), normAttr.data.size());
            geometry.attributes.push_back(normAttr);
        }

        // UVs if available
        if (!uvs.empty()) {
            GeometryAttribute uvAttr;
            uvAttr.name = "TEXCOORD_0";
            uvAttr.count = uvs.size();
            uvAttr.format = VK_FORMAT_R32G32_SFLOAT;
            uvAttr.data.resize(uvs.size() * sizeof(glm::vec2));
            memcpy(uvAttr.data.data(), uvs.data(), uvAttr.data.size());
            geometry.attributes.push_back(uvAttr);
        }

        // Indices
        geometry.indicesData = std::vector<std::byte>(indices.size() * sizeof(uint32_t));
        memcpy(geometry.indicesData->data(), indices.data(), geometry.indicesData->size());
        pure::GeometryIndicesMeta indicesMeta;
        indicesMeta.count = indices.size();
        indicesMeta.indexType = IndexType::U32;
        geometry.indices = indicesMeta;

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

#else

#include "fbx/import/FBXGeometry.h"

namespace fbx
{
    void ProcessMesh(FbxMesh* /*mesh*/, FBXModel& /*model*/) {}
}

#endif
