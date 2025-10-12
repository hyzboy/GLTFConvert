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
#include "common/GeometryAttribute.h"
#include "common/PrimitiveType.h"
#include "common/IndexType.h"
#include "common/VkFormat.h"

namespace fbx
{
    void ProcessMesh(fbxsdk::FbxMesh* mesh, fbxsdk::FbxNode* node, FBXModel& model)
    {
        // Ensure mesh is triangulated first
        fbxsdk::FbxGeometryConverter converter(mesh->GetFbxManager());
        converter.Triangulate(mesh, true);

        // Build material mapping from node materials to model.materials
        std::vector<int> materialMap; // index by node material index -> model.materials index
        if (node)
        {
            int matCount = node->GetMaterialCount();
            materialMap.resize(matCount, -1);
            for (int i = 0; i < matCount; ++i)
            {
                fbxsdk::FbxSurfaceMaterial* m = node->GetMaterial(i);
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
                        // Extract raw FBX material
                        pure::FBXMaterial raw;
                        std::optional<pure::PBRMaterial> pbrEstimate;
                        ExtractMaterial(m, raw, &pbrEstimate);

                        int matIndex = -1;
                        // Prefer explicit PBR estimate if available
                        if (pbrEstimate)
                        {
                            auto pm = std::make_unique<pure::PBRMaterial>(*pbrEstimate);
                            pm->name = raw.name;
                            model.materials.push_back(std::move(pm));
                            matIndex = static_cast<int>(model.materials.size()) - 1;
                        }
                        else if (raw.impl == "Phong")
                        {
                            auto ph = std::make_unique<pure::PhongMaterial>();
                            ph->name = raw.name;
                            if (raw.raw.contains("Diffuse") && raw.raw["Diffuse"].is_array()) {
                                auto &a = raw.raw["Diffuse"];
                                if (a.size() >= 3) {
                                    ph->diffuse[0] = static_cast<float>(a[0].get<double>());
                                    ph->diffuse[1] = static_cast<float>(a[1].get<double>());
                                    ph->diffuse[2] = static_cast<float>(a[2].get<double>());
                                }
                            }
                            if (raw.raw.contains("Specular") && raw.raw["Specular"].is_array()) {
                                auto &a = raw.raw["Specular"];
                                if (a.size() >= 3) {
                                    ph->specular[0] = static_cast<float>(a[0].get<double>());
                                    ph->specular[1] = static_cast<float>(a[1].get<double>());
                                    ph->specular[2] = static_cast<float>(a[2].get<double>());
                                }
                            }
                            if (raw.raw.contains("Shininess") ) ph->shininess = static_cast<float>(raw.raw["Shininess"].get<double>());
                            model.materials.push_back(std::move(ph));
                            matIndex = static_cast<int>(model.materials.size()) - 1;
                        }
                        else if (raw.impl == "Lambert")
                        {
                            auto lm = std::make_unique<pure::LambertMaterial>();
                            lm->name = raw.name;
                            if (raw.raw.contains("Diffuse") && raw.raw["Diffuse"].is_array()) {
                                auto &a = raw.raw["Diffuse"];
                                if (a.size() >= 3) {
                                    lm->diffuse[0] = static_cast<float>(a[0].get<double>());
                                    lm->diffuse[1] = static_cast<float>(a[1].get<double>());
                                    lm->diffuse[2] = static_cast<float>(a[2].get<double>());
                                }
                            }
                            model.materials.push_back(std::move(lm));
                            matIndex = static_cast<int>(model.materials.size()) - 1;
                        }
                        else if (raw.raw.contains("Glossiness") || raw.raw.contains("glossiness") || raw.raw.contains("GlossinessFactor"))
                        {
                            auto sg = std::make_unique<pure::SpecGlossMaterial>();
                            sg->name = raw.name;
                            if (raw.raw.contains("Diffuse") && raw.raw["Diffuse"].is_array()) {
                                auto &a = raw.raw["Diffuse"];
                                if (a.size() >= 3) {
                                    sg->diffuse[0] = static_cast<float>(a[0].get<double>());
                                    sg->diffuse[1] = static_cast<float>(a[1].get<double>());
                                    sg->diffuse[2] = static_cast<float>(a[2].get<double>());
                                }
                            }
                            if (raw.raw.contains("Specular") && raw.raw["Specular"].is_array()) {
                                auto &a = raw.raw["Specular"];
                                if (a.size() >= 3) {
                                    sg->specular[0] = static_cast<float>(a[0].get<double>());
                                    sg->specular[1] = static_cast<float>(a[1].get<double>());
                                    sg->specular[2] = static_cast<float>(a[2].get<double>());
                                }
                            }
                            if (raw.raw.contains("Glossiness")) sg->glossiness = static_cast<float>(raw.raw["Glossiness"].get<double>());
                            model.materials.push_back(std::move(sg));
                            matIndex = static_cast<int>(model.materials.size()) - 1;
                        }
                        else
                        {
                            // Fallback: store raw FBX material
                            auto fb = std::make_unique<pure::FBXMaterial>();
                            fb->name = raw.name;
                            fb->impl = raw.impl;
                            fb->raw = raw.raw;
                            fb->textures = std::move(raw.textures);
                            model.materials.push_back(std::move(fb));
                            matIndex = static_cast<int>(model.materials.size()) - 1;
                        }

                        found = matIndex;
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
        fbxsdk::FbxGeometryElementNormal* normalElement = mesh->GetElementNormal();
        if (normalElement)
        {
            auto m = normalElement->GetMappingMode();
            if (m == fbxsdk::FbxLayerElement::eByPolygonVertex) hasByPolygonVertex = true;
            else if (m == fbxsdk::FbxLayerElement::eByControlPoint) hasByControlPoint = true;
            else if (m == fbxsdk::FbxLayerElement::eByPolygon) hasByPolygon = true;
            else if (m == fbxsdk::FbxLayerElement::eByEdge) { std::cerr << "Unsupported mapping mode eByEdge for normals on mesh " << (mesh->GetName()?mesh->GetName():"(unnamed)") << std::endl; return; }
        }

        // UVs
        const int uvElementCount = mesh->GetElementUVCount();
        for (int uvIdx = 0; uvIdx < uvElementCount; ++uvIdx)
        {
            fbxsdk::FbxGeometryElementUV* uvElement = mesh->GetElementUV(uvIdx);
            if (!uvElement) continue;
            auto m = uvElement->GetMappingMode();
            if (m == fbxsdk::FbxLayerElement::eByPolygonVertex) hasByPolygonVertex = true;
            else if (m == fbxsdk::FbxLayerElement::eByControlPoint) hasByControlPoint = true;
            else if (m == fbxsdk::FbxLayerElement::eByPolygon) hasByPolygon = true;
            else if (m == fbxsdk::FbxLayerElement::eByEdge) { std::cerr << "Unsupported mapping mode eByEdge for UV on mesh " << (mesh->GetName()?mesh->GetName():"(unnamed)") << std::endl; return; }
        }

        // Tangent / binormal
        fbxsdk::FbxGeometryElementTangent* tanElement = mesh->GetElementTangent();
        if (tanElement)
        {
            auto m = tanElement->GetMappingMode();
            if (m == fbxsdk::FbxLayerElement::eByPolygonVertex) hasByPolygonVertex = true;
            else if (m == fbxsdk::FbxLayerElement::eByControlPoint) hasByControlPoint = true;
            else if (m == fbxsdk::FbxLayerElement::eByPolygon) hasByPolygon = true;
            else if (m == fbxsdk::FbxLayerElement::eByEdge) { std::cerr << "Unsupported mapping mode eByEdge for tangent on mesh " << (mesh->GetName()?mesh->GetName():"(unnamed)") << std::endl; return; }
        }
        fbxsdk::FbxGeometryElementBinormal* binElement = mesh->GetElementBinormal();
        if (binElement)
        {
            auto m2 = binElement->GetMappingMode();
            if (m2 == fbxsdk::FbxLayerElement::eByPolygonVertex) hasByPolygonVertex = true;
            else if (m2 == fbxsdk::FbxLayerElement::eByControlPoint) hasByControlPoint = true;
            else if (m2 == fbxsdk::FbxLayerElement::eByPolygon) hasByPolygon = true;
            else if (m2 == fbxsdk::FbxLayerElement::eByEdge) { std::cerr << "Unsupported mapping mode eByEdge for binormal on mesh " << (mesh->GetName()?mesh->GetName():"(unnamed)") << std::endl; return; }
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
