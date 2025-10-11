#include "SceneExportCollectTextures.h"
#include "SceneExportCollect.h"
#include "pure/Model.h"
#include "gltf/GLTFMaterial.h"
#include "gltf/GLTFMaterialImpl.h"
#include <unordered_set>

namespace exporters
{
    void CollectUsedTextures(const pure::Model &model,
                             const CollectedIndices &ci,
                             std::vector<std::size_t> &outTextures,
                             std::vector<std::size_t> &outImages,
                             std::vector<std::size_t> &outSamplers)
    {
        std::unordered_set<std::size_t> texSet, imgSet, sampSet;

        for (int32_t mid : ci.materials)
        {
            if (mid < 0 || mid >= static_cast<int32_t>(model.materials.size()))
                continue;

            const gltf::GLTFMaterialImpl *mat = dynamic_cast<const gltf::GLTFMaterialImpl*>(model.materials[mid].get());
            if (!mat) continue;
            const auto& gm = mat->gltfMaterial;

            auto addRef = [&](const GLTFTextureRef &ref)
            {
                if (!ref.index) return;
                std::size_t ti = *ref.index;
                if (ti >= model.textures.size()) return;
                if (!texSet.insert(ti).second) return; // already processed

                const auto &tiObj = model.textures[ti];
                if (tiObj.image && *tiObj.image < model.images.size())
                    imgSet.insert(*tiObj.image);
                if (tiObj.sampler && *tiObj.sampler < model.samplers.size())
                    sampSet.insert(*tiObj.sampler);
            };

            addRef(gm.pbr.baseColorTexture);
            addRef(gm.pbr.metallicRoughnessTexture);
            addRef(gm.normalTexture);
            addRef(gm.occlusionTexture);
            addRef(gm.emissiveTexture);
            if (gm.extTransmission)        addRef(gm.extTransmission->transmissionTexture);
            if (gm.extDiffuseTransmission) addRef(gm.extDiffuseTransmission->diffuseTransmissionTexture);
            if (gm.extSpecular)
            {
                addRef(gm.extSpecular->specularTexture);
                addRef(gm.extSpecular->specularColorTexture);
            }
            if (gm.extClearcoat)
            {
                addRef(gm.extClearcoat->clearcoatTexture);
                addRef(gm.extClearcoat->clearcoatRoughnessTexture);
                addRef(gm.extClearcoat->clearcoatNormalTexture);
            }
            if (gm.extSheen)
            {
                addRef(gm.extSheen->sheenColorTexture);
                addRef(gm.extSheen->sheenRoughnessTexture);
            }
            if (gm.extVolume)      addRef(gm.extVolume->thicknessTexture);
            if (gm.extAnisotropy)  addRef(gm.extAnisotropy->texture);
            if (gm.extIridescence)
            {
                addRef(gm.extIridescence->texture);
                addRef(gm.extIridescence->thicknessTexture);
            }
        }

        outTextures.assign(texSet.begin(), texSet.end());
        outImages.assign(imgSet.begin(), imgSet.end());
        outSamplers.assign(sampSet.begin(), sampSet.end());
    }
}
