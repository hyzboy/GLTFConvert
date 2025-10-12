#include "fbx/import/FBXMaterialMap.h"
#include <fbxsdk.h>
#include <string>
#include <memory>
#include <optional>
#include "pure/PBRMaterial.h"
#include "pure/FBXMaterial.h"
#include "pure/LambertMaterial.h"
#include "pure/PhongMaterial.h"
#include "pure/SpecGlossMaterial.h"
#include "fbx/import/FBXMaterialConverter.h"

namespace fbx {

void BuildMaterialMap(fbxsdk::FbxNode* node, ::FBXModel& model, std::vector<int>& materialMap)
{
    materialMap.clear();
    if (!node) return;
    int matCount = node->GetMaterialCount();
    materialMap.resize(matCount, -1);
    for (int i = 0; i < matCount; ++i)
    {
        fbxsdk::FbxSurfaceMaterial* m = node->GetMaterial(i);
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
                pure::FBXMaterial raw;
                std::optional<pure::PBRMaterial> pbrEstimate;
                ExtractMaterial(m, raw, &pbrEstimate);

                int matIndex = -1;
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
                        auto a = raw.raw["Diffuse"];
                        if (a.size() >= 3) {
                            ph->diffuse[0] = static_cast<float>(a[0].get<double>());
                            ph->diffuse[1] = static_cast<float>(a[1].get<double>());
                            ph->diffuse[2] = static_cast<float>(a[2].get<double>());
                        }
                    }
                    if (raw.raw.contains("Specular") && raw.raw["Specular"].is_array()) {
                        auto a = raw.raw["Specular"];
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
                        auto a = raw.raw["Diffuse"];
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
                        auto a = raw.raw["Diffuse"];
                        if (a.size() >= 3) {
                            sg->diffuse[0] = static_cast<float>(a[0].get<double>());
                            sg->diffuse[1] = static_cast<float>(a[1].get<double>());
                            sg->diffuse[2] = static_cast<float>(a[2].get<double>());
                        }
                    }
                    if (raw.raw.contains("Specular") && raw.raw["Specular"].is_array()) {
                        auto a = raw.raw["Specular"];
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

} // namespace fbx
