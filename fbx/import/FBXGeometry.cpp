#include "fbx/import/FBXGeometry.h"
#include "fbx/import/FBXImporter.h"

#include <vector>
#include <glm/glm.hpp>
#include <limits>
#include <memory>
#include "pure/Model.h"
#include "pure/Geometry.h"
#include "pure/Primitive.h"
#include "pure/PBRMaterial.h"
#include "common/GeometryAttribute.h"
#include "common/PrimitiveType.h"
#include "common/IndexType.h"
#include "common/VkFormat.h"

namespace fbx
{
    void ProcessMesh(FbxMesh* mesh, FbxNode* node, FBXModel& model)
    {
        // Ensure mesh is triangulated first
        FbxGeometryConverter converter(mesh->GetFbxManager());
        converter.Triangulate(mesh, true);

        // Build material mapping from node materials to model.materials
        std::vector<int> materialMap; // index by node material index -> model.materials index
        if (node)
        {
            int matCount = node->GetMaterialCount();
            materialMap.resize(matCount, -1);
            for (int i = 0; i < matCount; ++i)
            {
                FbxSurfaceMaterial* m = node->GetMaterial(i);
                // Simple mapping: use pointer address as key by searching existing materials by name
                int found = -1;
                if (m)
                {
                    std::string mname = m->GetName() ? m->GetName() : std::string();
                    for (size_t mi = 0; mi < model.materials.size(); ++mi)
                    {
                        if (model.materials[mi] && model.materials[mi]->name == mname) { found = static_cast<int>(mi); break; }
                    }
                    if (found == -1)
                    {
                        // Create a simple PBRMaterial placeholder
                        auto pm = std::make_unique<pure::PBRMaterial>();
                        pm->name = mname;
                        found = static_cast<int>(model.materials.size());
                        model.materials.push_back(std::move(pm));
                    }
                }
                materialMap[i] = found;
            }
        }

        // Detect mapping modes to pick expansion strategy
        bool hasByPolygonVertex = false;
        bool hasByControlPoint = false;

        // Normals
        FbxGeometryElementNormal* normalElement = mesh->GetElementNormal();
        if (normalElement)
        {
            if (normalElement->GetMappingMode() == FbxLayerElement::eByPolygonVertex) hasByPolygonVertex = true;
            if (normalElement->GetMappingMode() == FbxLayerElement::eByControlPoint) hasByControlPoint = true;
        }

        // UVs
        const int uvElementCount = mesh->GetElementUVCount();
        for (int uvIdx = 0; uvIdx < uvElementCount; ++uvIdx)
        {
            FbxGeometryElementUV* uvElement = mesh->GetElementUV(uvIdx);
            if (!uvElement) continue;
            if (uvElement->GetMappingMode() == FbxLayerElement::eByPolygonVertex) hasByPolygonVertex = true;
            if (uvElement->GetMappingMode() == FbxLayerElement::eByControlPoint) hasByControlPoint = true;
        }

        // Tangent / binormal
        FbxGeometryElementTangent* tanElement = mesh->GetElementTangent();
        if (tanElement)
        {
            if (tanElement->GetMappingMode() == FbxLayerElement::eByPolygonVertex) hasByPolygonVertex = true;
            if (tanElement->GetMappingMode() == FbxLayerElement::eByControlPoint) hasByControlPoint = true;
        }
        FbxGeometryElementBinormal* binElement = mesh->GetElementBinormal();
        if (binElement)
        {
            if (binElement->GetMappingMode() == FbxLayerElement::eByPolygonVertex) hasByPolygonVertex = true;
            if (binElement->GetMappingMode() == FbxLayerElement::eByControlPoint) hasByControlPoint = true;
        }

        // Choose strategy: polygon-vertex has highest priority, then control point, otherwise per-polygon fallback
        if (hasByPolygonVertex)
        {
            Expand_ByPolygonVertex(mesh, model, materialMap);
        }
        else if (hasByControlPoint)
        {
            Expand_ByControlPoint(mesh, model, materialMap);
        }
        else
        {
            Expand_ByPolygon(mesh, model, materialMap);
        }
    }
}
