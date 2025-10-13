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
                auto concrete = ExtractMaterial(m, raw);
                int matIndex = -1;
                if (concrete) {
                    model.materials.push_back(std::move(concrete));
                    matIndex = static_cast<int>(model.materials.size()) - 1;
                } else {
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
