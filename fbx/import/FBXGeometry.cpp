#include "fbx/import/FBXGeometry.h"
#include "fbx/import/FBXImporter.h"

#include <vector>
#include <glm/glm.hpp>
#include <limits>
#include <memory>
#include <iostream>
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
        bool hasByPolygon = false;

        // Normals
        FbxGeometryElementNormal* normalElement = mesh->GetElementNormal();
        if (normalElement)
        {
            auto m = normalElement->GetMappingMode();
            if (m == FbxLayerElement::eByPolygonVertex) hasByPolygonVertex = true;
            else if (m == FbxLayerElement::eByControlPoint) hasByControlPoint = true;
            else if (m == FbxLayerElement::eByPolygon) hasByPolygon = true;
            else if (m == FbxLayerElement::eByEdge) { std::cerr << "Unsupported mapping mode eByEdge for normals on mesh " << (mesh->GetName()?mesh->GetName():"(unnamed)") << std::endl; return; }
        }

        // UVs
        const int uvElementCount = mesh->GetElementUVCount();
        for (int uvIdx = 0; uvIdx < uvElementCount; ++uvIdx)
        {
            FbxGeometryElementUV* uvElement = mesh->GetElementUV(uvIdx);
            if (!uvElement) continue;
            auto m = uvElement->GetMappingMode();
            if (m == FbxLayerElement::eByPolygonVertex) hasByPolygonVertex = true;
            else if (m == FbxLayerElement::eByControlPoint) hasByControlPoint = true;
            else if (m == FbxLayerElement::eByPolygon) hasByPolygon = true;
            else if (m == FbxLayerElement::eByEdge) { std::cerr << "Unsupported mapping mode eByEdge for UV on mesh " << (mesh->GetName()?mesh->GetName():"(unnamed)") << std::endl; return; }
        }

        // Tangent / binormal
        FbxGeometryElementTangent* tanElement = mesh->GetElementTangent();
        if (tanElement)
        {
            auto m = tanElement->GetMappingMode();
            if (m == FbxLayerElement::eByPolygonVertex) hasByPolygonVertex = true;
            else if (m == FbxLayerElement::eByControlPoint) hasByControlPoint = true;
            else if (m == FbxLayerElement::eByPolygon) hasByPolygon = true;
            else if (m == FbxLayerElement::eByEdge) { std::cerr << "Unsupported mapping mode eByEdge for tangent on mesh " << (mesh->GetName()?mesh->GetName():"(unnamed)") << std::endl; return; }
        }
        FbxGeometryElementBinormal* binElement = mesh->GetElementBinormal();
        if (binElement)
        {
            auto m2 = binElement->GetMappingMode();
            if (m2 == FbxLayerElement::eByPolygonVertex) hasByPolygonVertex = true;
            else if (m2 == FbxLayerElement::eByControlPoint) hasByControlPoint = true;
            else if (m2 == FbxLayerElement::eByPolygon) hasByPolygon = true;
            else if (m2 == FbxLayerElement::eByEdge) { std::cerr << "Unsupported mapping mode eByEdge for binormal on mesh " << (mesh->GetName()?mesh->GetName():"(unnamed)") << std::endl; return; }
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
