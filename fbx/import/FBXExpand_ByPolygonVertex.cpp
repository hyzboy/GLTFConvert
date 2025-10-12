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
    void Expand_ByPolygonVertex(FbxMesh *mesh,FBXModel &model,const std::vector<int> &materialMap)
    {
        // Mesh is expected to be triangulated by the caller (ProcessMesh). Do not triangulate here.

        int vertexCount = mesh->GetControlPointsCount();
        FbxVector4 *controlPoints = mesh->GetControlPoints();

        int polygonCount = mesh->GetPolygonCount();

        // We'll build a mapping (controlPoint, attrIndices...) -> new vertex index
        struct VertexKey
        {
            int cpIndex;
            std::vector<int> attrIndices; // one per layer: normalIdx, uvIdx0, uvIdx1, tangentIdx, etc.
            bool operator<(VertexKey const &o) const {
                if(cpIndex != o.cpIndex) return cpIndex < o.cpIndex;
                return attrIndices < o.attrIndices;
            }
        };

        std::map<VertexKey,int> vertexMap;
        std::vector<glm::vec3> outPositions;
        // Store attribute data as tightly-packed float arrays to avoid extra
        // conversions and potential glm padding issues.
        std::vector<float> outNormalsData;                  // 3 floats per vertex
        std::vector<std::vector<float>> outUVsData;         // per UV layer: 2 floats per vertex
        std::vector<float> outTangentsData;                 // 4 floats per vertex
        std::vector<float> outBinormalsData;               // 3 floats per vertex
        std::vector<uint32_t> outIndices;

        // Prepare UV layer count
        int uvLayerCount = mesh->GetElementUVCount();
        outUVsData.resize(uvLayerCount);

        // iterate polygons and polygon-vertices
        for(int p = 0; p < polygonCount; ++p)
        {
            int polySize = mesh->GetPolygonSize(p);
            for(int v = 0; v < polySize && v < 3; ++v)
            {
                int cpIndex = mesh->GetPolygonVertex(p,v);

                VertexKey key;
                key.cpIndex = cpIndex;

                // gather normal index if present and mapped by polygon-vertex
                FbxGeometryElementNormal *normalElement = mesh->GetElementNormal();
                if(normalElement && normalElement->GetMappingMode() == FbxLayerElement::eByPolygonVertex)
                {
                    int idx = -1;
                    if(normalElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
                        idx = normalElement->GetIndexArray().GetAt(mesh->GetPolygonVertexIndex(p) + v);
                    else
                        idx = mesh->GetPolygonVertexIndex(p) + v;
                    key.attrIndices.push_back(idx);
                }
                else key.attrIndices.push_back(-1);

                // UVs
                for(int uvL = 0; uvL < uvLayerCount; ++uvL)
                {
                    FbxGeometryElementUV *uvElement = mesh->GetElementUV(uvL);
                    if(uvElement && uvElement->GetMappingMode() == FbxLayerElement::eByPolygonVertex)
                    {
                        int idx = -1;
                        if(uvElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
                            idx = uvElement->GetIndexArray().GetAt(mesh->GetPolygonVertexIndex(p) + v);
                        else
                            idx = mesh->GetPolygonVertexIndex(p) + v;
                        key.attrIndices.push_back(idx);
                    }
                    else key.attrIndices.push_back(-1);
                }

                // tangent/binormal similar (not fully robust here)
                FbxGeometryElementTangent *tanElement = mesh->GetElementTangent();
                if(tanElement && tanElement->GetMappingMode() == FbxLayerElement::eByPolygonVertex)
                {
                    int idx = -1;
                    if(tanElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
                        idx = tanElement->GetIndexArray().GetAt(mesh->GetPolygonVertexIndex(p) + v);
                    else
                        idx = mesh->GetPolygonVertexIndex(p) + v;
                    key.attrIndices.push_back(idx);
                }
                else key.attrIndices.push_back(-1);

                FbxGeometryElementBinormal *binElement = mesh->GetElementBinormal();
                if(binElement && binElement->GetMappingMode() == FbxLayerElement::eByPolygonVertex)
                {
                    int idx = -1;
                    if(binElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
                        idx = binElement->GetIndexArray().GetAt(mesh->GetPolygonVertexIndex(p) + v);
                    else
                        idx = mesh->GetPolygonVertexIndex(p) + v;
                    key.attrIndices.push_back(idx);
                }
                else key.attrIndices.push_back(-1);

                auto it = vertexMap.find(key);
                int outIdx;
                if(it == vertexMap.end())
                {
                    outIdx = static_cast<int>(outPositions.size());
                    vertexMap.emplace(key,outIdx);
                    // push position
                    FbxVector4 cp = mesh->GetControlPointAt(cpIndex);
                    outPositions.push_back({ static_cast<float>(cp[0]),static_cast<float>(cp[1]),static_cast<float>(cp[2]) });
                    // push normals/uvs/tangents if present by resolving direct arrays
                    // normal (pack directly into float array)
                    if(key.attrIndices.size() > 0 && key.attrIndices[0] >= 0)
                    {
                        FbxGeometryElementNormal *nElem = mesh->GetElementNormal();
                        if(nElem) {
                            FbxVector4 nv = nElem->GetDirectArray().GetAt(key.attrIndices[0]);
                            outNormalsData.push_back(static_cast<float>(nv[0]));
                            outNormalsData.push_back(static_cast<float>(nv[1]));
                            outNormalsData.push_back(static_cast<float>(nv[2]));
                        }
                        else { outNormalsData.push_back(0.0f); outNormalsData.push_back(0.0f); outNormalsData.push_back(0.0f); }
                    }
                    else { outNormalsData.push_back(0.0f); outNormalsData.push_back(0.0f); outNormalsData.push_back(0.0f); }

                    // UVs
                    for(int uvL = 0; uvL < uvLayerCount; ++uvL)
                    {
                        int uvIdx = key.attrIndices[1 + uvL];
                        if(uvIdx >= 0)
                        {
                            FbxGeometryElementUV *uElem = mesh->GetElementUV(uvL);
                            FbxVector2 uv = uElem->GetDirectArray().GetAt(uvIdx);
                            outUVsData[uvL].push_back(static_cast<float>(uv[0]));
                            outUVsData[uvL].push_back(static_cast<float>(uv[1]));
                        }
                        else
                        {
                            outUVsData[uvL].push_back(0.0f);
                            outUVsData[uvL].push_back(0.0f);
                        }
                    }

                    // tangents
                    int tanIdx = key.attrIndices[1 + uvLayerCount];
                    if(tanIdx >= 0)
                    {
                        FbxGeometryElementTangent *tElem = mesh->GetElementTangent();
                        FbxVector4 tv = tElem->GetDirectArray().GetAt(tanIdx);
                        outTangentsData.push_back(static_cast<float>(tv[0]));
                        outTangentsData.push_back(static_cast<float>(tv[1]));
                        outTangentsData.push_back(static_cast<float>(tv[2]));
                        outTangentsData.push_back(static_cast<float>(tv[3]));
                    }
                    else { outTangentsData.push_back(0.0f); outTangentsData.push_back(0.0f); outTangentsData.push_back(0.0f); outTangentsData.push_back(0.0f); }

                    int binIdx = key.attrIndices[2 + uvLayerCount];
                    if(binIdx >= 0)
                    {
                        FbxGeometryElementBinormal *bElem = mesh->GetElementBinormal();
                        FbxVector4 bv = bElem->GetDirectArray().GetAt(binIdx);
                        outBinormalsData.push_back(static_cast<float>(bv[0]));
                        outBinormalsData.push_back(static_cast<float>(bv[1]));
                        outBinormalsData.push_back(static_cast<float>(bv[2]));
                    }
                    else { outBinormalsData.push_back(0.0f); outBinormalsData.push_back(0.0f); outBinormalsData.push_back(0.0f); }
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
        // Pack tightly into float array to avoid potential glm::vec3 padding/stride issues
        if(!outPositions.empty()) {
            std::vector<float> posData;
            posData.reserve(outPositions.size() * 3);
            for(const auto &p : outPositions) {
                posData.push_back(p.x);
                posData.push_back(p.y);
                posData.push_back(p.z);
            }
            posAttr.data.resize(posData.size() * sizeof(float));
            memcpy(posAttr.data.data(),posData.data(),posAttr.data.size());
        }
        geometry.attributes.push_back(posAttr);

        if(!outNormalsData.empty()) {
            GeometryAttribute normAttr;
            normAttr.name = "NORMAL";
            normAttr.count = static_cast<int>(outPositions.size());
            normAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
            normAttr.data.resize(outNormalsData.size() * sizeof(float));
            memcpy(normAttr.data.data(), outNormalsData.data(), normAttr.data.size());
            geometry.attributes.push_back(normAttr);
        }

        for(int uvL = 0; uvL < uvLayerCount; ++uvL)
        {
            if(outUVsData[uvL].empty()) continue;
            GeometryAttribute uvAttr;
            uvAttr.name = std::string("TEXCOORD_") + std::to_string(uvL);
            uvAttr.count = static_cast<int>(outPositions.size());
            uvAttr.format = VK_FORMAT_R32G32_SFLOAT;
            uvAttr.data.resize(outUVsData[uvL].size() * sizeof(float));
            memcpy(uvAttr.data.data(), outUVsData[uvL].data(), uvAttr.data.size());
            geometry.attributes.push_back(uvAttr);
        }

        if(!outTangentsData.empty()) {
            GeometryAttribute tanAttr;
            tanAttr.name = "TANGENT";
            tanAttr.count = static_cast<int>(outPositions.size());
            tanAttr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
            tanAttr.data.resize(outTangentsData.size() * sizeof(float));
            memcpy(tanAttr.data.data(), outTangentsData.data(), tanAttr.data.size());
            geometry.attributes.push_back(tanAttr);
        }
        if(!outBinormalsData.empty()) {
            GeometryAttribute bitAttr;
            bitAttr.name = "BINORMAL";
            bitAttr.count = static_cast<int>(outPositions.size());
            bitAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
            bitAttr.data.resize(outBinormalsData.size() * sizeof(float));
            memcpy(bitAttr.data.data(), outBinormalsData.data(), bitAttr.data.size());
            geometry.attributes.push_back(bitAttr);
        }

        // indices storage
        const size_t vertexCountSize = outPositions.size();
        pure::GeometryIndicesMeta indicesMeta;
        indicesMeta.count = outIndices.size();
        if(g_allow_u8_indices && vertexCountSize <= static_cast<size_t>(std::numeric_limits<uint8_t>::max()))
        {
            std::vector<uint8_t> idx8;
            idx8.reserve(outIndices.size());
            for(uint32_t v : outIndices) idx8.push_back(static_cast<uint8_t>(v));
            geometry.indicesData = std::vector<std::byte>(idx8.size() * sizeof(uint8_t));
            memcpy(geometry.indicesData->data(),idx8.data(),geometry.indicesData->size());
            indicesMeta.indexType = IndexType::U8;
        }
        else if(vertexCountSize <= static_cast<size_t>(std::numeric_limits<uint16_t>::max()))
        {
            std::vector<uint16_t> idx16;
            idx16.reserve(outIndices.size());
            for(uint32_t v : outIndices) idx16.push_back(static_cast<uint16_t>(v));
            geometry.indicesData = std::vector<std::byte>(idx16.size() * sizeof(uint16_t));
            memcpy(geometry.indicesData->data(),idx16.data(),geometry.indicesData->size());
            indicesMeta.indexType = IndexType::U16;
        }
        else
        {
            geometry.indicesData = std::vector<std::byte>(outIndices.size() * sizeof(uint32_t));
            memcpy(geometry.indicesData->data(),outIndices.data(),geometry.indicesData->size());
            indicesMeta.indexType = IndexType::U32;
        }
        geometry.indices = indicesMeta;

        if(!outPositions.empty()) {
            glm::vec3 min = outPositions[0]; glm::vec3 max = outPositions[0];
            for(const auto &p : outPositions) { min = glm::min(min,p); max = glm::max(max,p); }
            geometry.bounding_volume.aabb.min = min; geometry.bounding_volume.aabb.max = max;
        }

        int32_t geomIndex = static_cast<int32_t>(model.geometry.size());
        model.geometry.push_back(std::move(geometry));

        pure::Primitive prim; prim.geometry = geomIndex;
        if(!materialMap.empty() && materialMap[0] >= 0) prim.material = static_cast<int32_t>(materialMap[0]);
        int32_t primIndex = static_cast<int32_t>(model.primitives.size()); model.primitives.push_back(prim);
        pure::Mesh pm; pm.name = mesh->GetName() ? mesh->GetName() : std::string(); pm.primitives.push_back(primIndex);
        if(!outPositions.empty()) { glm::vec3 mn = outPositions[0]; glm::vec3 mx = outPositions[0]; for(const auto &p : outPositions){ mn = glm::min(mn,p); mx = glm::max(mx,p); } pm.bounding_volume.aabb.min = mn; pm.bounding_volume.aabb.max = mx; }
        model.meshes.push_back(std::move(pm));
    }
}
