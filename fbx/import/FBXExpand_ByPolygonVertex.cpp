#include "fbx/import/FBXGeometry.h"
#include "fbx/import/FBXImporter.h"

#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
#include <limits>
#include <tuple>
#include <map>
#include "pure/Model.h"
#include "pure/Geometry.h"
#include "pure/Primitive.h"
#include "common/GeometryAttribute.h"
#include "common/PrimitiveType.h"
#include "common/IndexType.h"
#include "common/VkFormat.h"

#include <fbxsdk.h>
using namespace fbxsdk;

namespace fbx
{
    // Expand where attributes are mapped by polygon-vertex (per-face-vertex). We create unique vertices for each polygon-vertex combination and deduplicate by full attribute key.
    void Expand_ByPolygonVertex(FbxMesh* mesh, FBXModel& model, const std::vector<int> &materialMap)
    {
        // Triangulate
        FbxGeometryConverter converter(mesh->GetFbxManager());
        converter.Triangulate(mesh, true);

        int vertexCount = mesh->GetControlPointsCount();
        FbxVector4* controlPoints = mesh->GetControlPoints();

        int polygonCount = mesh->GetPolygonCount();

        // We'll build a mapping (controlPoint, attrIndices...) -> new vertex index
        struct VertexKey
        {
            int cpIndex;
            std::vector<int> attrIndices; // one per layer: normalIdx, uvIdx0, uvIdx1, tangentIdx, etc.
            bool operator<(VertexKey const& o) const {
                if (cpIndex != o.cpIndex) return cpIndex < o.cpIndex;
                return attrIndices < o.attrIndices;
            }
        };

        std::map<VertexKey,int> vertexMap;
        std::vector<glm::vec3> outPositions;
        std::vector<glm::vec3> outNormals;
        std::vector<std::vector<glm::vec2>> outUVs; // dynamic per uv set
        std::vector<glm::vec4> outTangents;
        std::vector<glm::vec3> outBinormals;
        std::vector<uint32_t> outIndices;

        // Prepare UV layer count
        int uvLayerCount = mesh->GetElementUVCount();
        outUVs.resize(uvLayerCount);

        // iterate polygons and polygon-vertices
        for (int p = 0; p < polygonCount; ++p)
        {
            int polySize = mesh->GetPolygonSize(p);
            for (int v = 0; v < polySize && v < 3; ++v)
            {
                int cpIndex = mesh->GetPolygonVertex(p, v);

                VertexKey key;
                key.cpIndex = cpIndex;

                // gather normal index if present and mapped by polygon-vertex
                FbxGeometryElementNormal* normalElement = mesh->GetElementNormal();
                if (normalElement && normalElement->GetMappingMode() == FbxLayerElement::eByPolygonVertex)
                {
                    int idx = -1;
                    if (normalElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
                        idx = normalElement->GetIndexArray().GetAt(mesh->GetPolygonVertexIndex(p) + v);
                    else
                        idx = mesh->GetPolygonVertexIndex(p) + v;
                    key.attrIndices.push_back(idx);
                }
                else key.attrIndices.push_back(-1);

                // UVs
                for (int uvL = 0; uvL < uvLayerCount; ++uvL)
                {
                    FbxGeometryElementUV* uvElement = mesh->GetElementUV(uvL);
                    if (uvElement && uvElement->GetMappingMode() == FbxLayerElement::eByPolygonVertex)
                    {
                        int idx = -1;
                        if (uvElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
                            idx = uvElement->GetIndexArray().GetAt(mesh->GetPolygonVertexIndex(p) + v);
                        else
                            idx = mesh->GetPolygonVertexIndex(p) + v;
                        key.attrIndices.push_back(idx);
                    }
                    else key.attrIndices.push_back(-1);
                }

                // tangent/binormal similar (not fully robust here)
                FbxGeometryElementTangent* tanElement = mesh->GetElementTangent();
                if (tanElement && tanElement->GetMappingMode() == FbxLayerElement::eByPolygonVertex)
                {
                    int idx = -1;
                    if (tanElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
                        idx = tanElement->GetIndexArray().GetAt(mesh->GetPolygonVertexIndex(p) + v);
                    else
                        idx = mesh->GetPolygonVertexIndex(p) + v;
                    key.attrIndices.push_back(idx);
                }
                else key.attrIndices.push_back(-1);

                FbxGeometryElementBinormal* binElement = mesh->GetElementBinormal();
                if (binElement && binElement->GetMappingMode() == FbxLayerElement::eByPolygonVertex)
                {
                    int idx = -1;
                    if (binElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
                        idx = binElement->GetIndexArray().GetAt(mesh->GetPolygonVertexIndex(p) + v);
                    else
                        idx = mesh->GetPolygonVertexIndex(p) + v;
                    key.attrIndices.push_back(idx);
                }
                else key.attrIndices.push_back(-1);

                auto it = vertexMap.find(key);
                int outIdx;
                if (it == vertexMap.end())
                {
                    outIdx = static_cast<int>(outPositions.size());
                    vertexMap.emplace(key, outIdx);
                    // push position
                    FbxVector4 cp = mesh->GetControlPointAt(cpIndex);
                    outPositions.push_back({static_cast<float>(cp[0]), static_cast<float>(cp[1]), static_cast<float>(cp[2])});
                    // push normals/uvs/tangents if present by resolving direct arrays
                    if (key.attrIndices.size() > 0 && key.attrIndices[0] >= 0)
                    {
                        // normal
                        FbxGeometryElementNormal* nElem = mesh->GetElementNormal();
                        if (nElem) {
                            FbxVector4 nv = nElem->GetDirectArray().GetAt(key.attrIndices[0]);
                            outNormals.push_back({static_cast<float>(nv[0]), static_cast<float>(nv[1]), static_cast<float>(nv[2])});
                        } else outNormals.push_back({0,0,0});
                    }
                    else outNormals.push_back({0,0,0});

                    // UVs
                    for (int uvL = 0; uvL < uvLayerCount; ++uvL)
                    {
                        int uvIdx = key.attrIndices[1 + uvL];
                        if (uvIdx >= 0)
                        {
                            FbxGeometryElementUV* uElem = mesh->GetElementUV(uvL);
                            FbxVector2 uv = uElem->GetDirectArray().GetAt(uvIdx);
                            outUVs[uvL].push_back({static_cast<float>(uv[0]), static_cast<float>(uv[1])});
                        }
                        else
                        {
                            outUVs[uvL].push_back({0,0});
                        }
                    }

                    // tangents
                    int tanIdx = key.attrIndices[1 + uvLayerCount];
                    if (tanIdx >= 0)
                    {
                        FbxGeometryElementTangent* tElem = mesh->GetElementTangent();
                        FbxVector4 tv = tElem->GetDirectArray().GetAt(tanIdx);
                        outTangents.push_back({static_cast<float>(tv[0]), static_cast<float>(tv[1]), static_cast<float>(tv[2]), static_cast<float>(tv[3])});
                    }
                    else outTangents.push_back({0,0,0,0});

                    int binIdx = key.attrIndices[2 + uvLayerCount];
                    if (binIdx >= 0)
                    {
                        FbxGeometryElementBinormal* bElem = mesh->GetElementBinormal();
                        FbxVector4 bv = bElem->GetDirectArray().GetAt(binIdx);
                        outBinormals.push_back({static_cast<float>(bv[0]), static_cast<float>(bv[1]), static_cast<float>(bv[2])});
                    }
                    else outBinormals.push_back({0,0,0});
                }
                else
                {
                    outIdx = it->second;
                }

                outIndices.push_back(static_cast<uint32_t>(outIdx));
            }
        }

        // Now create pure::Geometry from outPositions/out* arrays
        pure::Geometry geometry;
        geometry.primitiveType = PrimitiveType::Triangles;
        geometry.positions = outPositions;

        GeometryAttribute posAttr;
        posAttr.name = "POSITION";
        posAttr.count = outPositions.size();
        posAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
        posAttr.data.resize(outPositions.size() * 3 * sizeof(float));
        memcpy(posAttr.data.data(), outPositions.data(), outPositions.size() * 3 * sizeof(float));
        geometry.attributes.push_back(posAttr);

        if (!outNormals.empty()) {
            GeometryAttribute normAttr;
            normAttr.name = "NORMAL";
            normAttr.count = outNormals.size();
            normAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
            normAttr.data.resize(outNormals.size() * 3 * sizeof(float));
            memcpy(normAttr.data.data(), outNormals.data(), outNormals.size() * 3 * sizeof(float));
            geometry.attributes.push_back(normAttr);
        }

        for (int uvL = 0; uvL < uvLayerCount; ++uvL)
        {
            if (outUVs[uvL].empty()) continue;
            GeometryAttribute uvAttr;
            uvAttr.name = std::string("TEXCOORD_") + std::to_string(uvL);
            uvAttr.count = outUVs[uvL].size();
            uvAttr.format = VK_FORMAT_R32G32_SFLOAT;
            uvAttr.data.resize(outUVs[uvL].size() * 2 * sizeof(float));
            memcpy(uvAttr.data.data(), outUVs[uvL].data(), outUVs[uvL].size() * 2 * sizeof(float));
            geometry.attributes.push_back(uvAttr);
        }

        if (!outTangents.empty()) {
            GeometryAttribute tanAttr;
            tanAttr.name = "TANGENT";
            tanAttr.count = outTangents.size();
            tanAttr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
            tanAttr.data.resize(outTangents.size() * 4 * sizeof(float));
            memcpy(tanAttr.data.data(), outTangents.data(), outTangents.size() * 4 * sizeof(float));
            geometry.attributes.push_back(tanAttr);
        }
        if (!outBinormals.empty()) {
            GeometryAttribute bitAttr;
            bitAttr.name = "BINORMAL";
            bitAttr.count = outBinormals.size();
            bitAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
            bitAttr.data.resize(outBinormals.size() * 3 * sizeof(float));
            memcpy(bitAttr.data.data(), outBinormals.data(), outBinormals.size() * 3 * sizeof(float));
            geometry.attributes.push_back(bitAttr);
        }

        // indices storage
        const size_t vertexCountSize = outPositions.size();
        pure::GeometryIndicesMeta indicesMeta;
        indicesMeta.count = outIndices.size();
        if (g_allow_u8_indices && vertexCountSize <= static_cast<size_t>(std::numeric_limits<uint8_t>::max()))
        {
            std::vector<uint8_t> idx8;
            idx8.reserve(outIndices.size());
            for (uint32_t v : outIndices) idx8.push_back(static_cast<uint8_t>(v));
            geometry.indicesData = std::vector<std::byte>(idx8.size() * sizeof(uint8_t));
            memcpy(geometry.indicesData->data(), idx8.data(), geometry.indicesData->size());
            indicesMeta.indexType = IndexType::U8;
        }
        else if (vertexCountSize <= static_cast<size_t>(std::numeric_limits<uint16_t>::max()))
        {
            std::vector<uint16_t> idx16;
            idx16.reserve(outIndices.size());
            for (uint32_t v : outIndices) idx16.push_back(static_cast<uint16_t>(v));
            geometry.indicesData = std::vector<std::byte>(idx16.size() * sizeof(uint16_t));
            memcpy(geometry.indicesData->data(), idx16.data(), geometry.indicesData->size());
            indicesMeta.indexType = IndexType::U16;
        }
        else
        {
            geometry.indicesData = std::vector<std::byte>(outIndices.size() * sizeof(uint32_t));
            memcpy(geometry.indicesData->data(), outIndices.data(), geometry.indicesData->size());
            indicesMeta.indexType = IndexType::U32;
        }
        geometry.indices = indicesMeta;

        if (!outPositions.empty()) {
            glm::vec3 min = outPositions[0]; glm::vec3 max = outPositions[0];
            for (const auto &p : outPositions) { min = glm::min(min, p); max = glm::max(max, p); }
            geometry.bounding_volume.aabb.min = min; geometry.bounding_volume.aabb.max = max;
        }

        int32_t geomIndex = static_cast<int32_t>(model.geometry.size());
        model.geometry.push_back(std::move(geometry));

        pure::Primitive prim; prim.geometry = geomIndex;
        if (!materialMap.empty() && materialMap[0] >= 0) prim.material = static_cast<int32_t>(materialMap[0]);
        int32_t primIndex = static_cast<int32_t>(model.primitives.size()); model.primitives.push_back(prim);
        pure::Mesh pm; pm.name = mesh->GetName()?mesh->GetName():std::string(); pm.primitives.push_back(primIndex);
        if (!outPositions.empty()) { glm::vec3 mn = outPositions[0]; glm::vec3 mx = outPositions[0]; for (const auto &p:outPositions){mn = glm::min(mn,p); mx = glm::max(mx,p);} pm.bounding_volume.aabb.min = mn; pm.bounding_volume.aabb.max = mx; }
        model.meshes.push_back(std::move(pm));
    }
}
