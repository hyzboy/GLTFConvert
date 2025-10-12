#include "fbx/import/FBXGeometry.h"
#include <fbxsdk.h>
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
#include "pure/FBXMaterial.h"
#include "pure/LambertMaterial.h"
#include "pure/PhongMaterial.h"
#include "pure/SpecGlossMaterial.h"
#include "fbx/import/FBXMaterialConverter.h"
#include "fbx/import/FBXMaterialMap.h"
#include "common/GeometryAttribute.h"
#include "common/PrimitiveType.h"
#include "common/IndexType.h"
#include "common/VkFormat.h"

namespace fbx
{
    void ProcessMesh(fbxsdk::FbxMesh* mesh, fbxsdk::FbxNode* node, FBXModel& model)
    {
        if (!mesh) {
            std::cerr << "Error: Null mesh pointer" << std::endl;
            return;
        }

        // Triangulate safely: Triangulation with replace=true may replace the original
        // FbxMesh object and leave the incoming pointer dangling. Triangulate using
        // the mesh API and re-acquire the mesh from its owning node if replaced.
        fbxsdk::FbxNode* ownerNode = mesh->GetNode();
        fbxsdk::FbxGeometryConverter converter(mesh->GetFbxManager());
        bool triOk = converter.Triangulate(mesh, true);
        if (ownerNode) {
            fbxsdk::FbxMesh* newMesh = ownerNode->GetMesh();
            if (newMesh) mesh = newMesh;
        }
        if (!triOk) {
            std::cerr << "Error: Triangulation failed for mesh " << (mesh && mesh->GetName()?mesh->GetName():"(unnamed)") << std::endl;
            return;
        }
        if (!mesh) {
            std::cerr << "Error: Mesh lost after triangulation" << std::endl;
            return;
        }

        std::vector<int> materialMap; // index by node material index -> model.materials index
        BuildMaterialMap(node, model, materialMap);

        // Detect mapping modes to pick expansion strategy
        bool hasByPolygonVertex = false;
        bool hasByControlPoint = false;
        bool hasByPolygon = false;

        // Normals (defensive: element may be null or SDK calls may fail)
        try {
            fbxsdk::FbxGeometryElementNormal* normalElement = mesh ? mesh->GetElementNormal() : nullptr;
            if (normalElement)
            {
                auto m = normalElement->GetMappingMode();
                if (m == fbxsdk::FbxLayerElement::eByPolygonVertex) hasByPolygonVertex = true;
                else if (m == fbxsdk::FbxLayerElement::eByControlPoint) hasByControlPoint = true;
                else if (m == fbxsdk::FbxLayerElement::eByPolygon) hasByPolygon = true;
                else if (m == fbxsdk::FbxLayerElement::eByEdge) {
                    std::cerr << "Unsupported mapping mode eByEdge for normals on mesh " << (mesh && mesh->GetName()?mesh->GetName():"(unnamed)") << std::endl;
                    return;
                }
            }
        }
        catch (const std::exception &e) {
            std::cerr << "Warning: exception while querying mesh normals: " << e.what() << std::endl;
            // treat as no normal element
        }
        catch (...) {
            std::cerr << "Warning: unknown exception while querying mesh normals" << std::endl;
        }

        // UVs
        const int uvElementCount = mesh->GetElementUVCount();
        for (int uvIdx = 0; uvIdx < uvElementCount; ++uvIdx)
        {
            try {
                fbxsdk::FbxGeometryElementUV* uvElement = mesh ? mesh->GetElementUV(uvIdx) : nullptr;
                if (!uvElement) continue;
                auto m = uvElement->GetMappingMode();
                if (m == fbxsdk::FbxLayerElement::eByPolygonVertex) hasByPolygonVertex = true;
                else if (m == fbxsdk::FbxLayerElement::eByControlPoint) hasByControlPoint = true;
                else if (m == fbxsdk::FbxLayerElement::eByPolygon) hasByPolygon = true;
                else if (m == fbxsdk::FbxLayerElement::eByEdge) {
                    std::cerr << "Unsupported mapping mode eByEdge for UV on mesh " << (mesh && mesh->GetName()?mesh->GetName():"(unnamed)") << std::endl;
                    return;
                }
            }
            catch (const std::exception &e) {
                std::cerr << "Warning: exception while querying UV element " << uvIdx << " : " << e.what() << std::endl;
            }
            catch (...) {
                std::cerr << "Warning: unknown exception while querying UV element " << uvIdx << std::endl;
            }
        }

        // Tangent / binormal
        try {
            fbxsdk::FbxGeometryElementTangent* tanElement = mesh ? mesh->GetElementTangent() : nullptr;
            if (tanElement)
            {
                auto m = tanElement->GetMappingMode();
                if (m == fbxsdk::FbxLayerElement::eByPolygonVertex) hasByPolygonVertex = true;
                else if (m == fbxsdk::FbxLayerElement::eByControlPoint) hasByControlPoint = true;
                else if (m == fbxsdk::FbxLayerElement::eByPolygon) hasByPolygon = true;
                else if (m == fbxsdk::FbxLayerElement::eByEdge) { std::cerr << "Unsupported mapping mode eByEdge for tangent on mesh " << (mesh && mesh->GetName()?mesh->GetName():"(unnamed)") << std::endl; return; }
            }
        }
        catch (const std::exception &e) {
            std::cerr << "Warning: exception while querying mesh tangents: " << e.what() << std::endl;
        }
        catch (...) {
            std::cerr << "Warning: unknown exception while querying mesh tangents" << std::endl;
        }

        try {
            fbxsdk::FbxGeometryElementBinormal* binElement = mesh ? mesh->GetElementBinormal() : nullptr;
            if (binElement)
            {
                auto m2 = binElement->GetMappingMode();
                if (m2 == fbxsdk::FbxLayerElement::eByPolygonVertex) hasByPolygonVertex = true;
                else if (m2 == fbxsdk::FbxLayerElement::eByControlPoint) hasByControlPoint = true;
                else if (m2 == fbxsdk::FbxLayerElement::eByPolygon) hasByPolygon = true;
                else if (m2 == fbxsdk::FbxLayerElement::eByEdge) { std::cerr << "Unsupported mapping mode eByEdge for binormal on mesh " << (mesh && mesh->GetName()?mesh->GetName():"(unnamed)") << std::endl; return; }
            }
        }
        catch (const std::exception &e) {
            std::cerr << "Warning: exception while querying mesh binormals: " << e.what() << std::endl;
        }
        catch (...) {
            std::cerr << "Warning: unknown exception while querying mesh binormals" << std::endl;
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
