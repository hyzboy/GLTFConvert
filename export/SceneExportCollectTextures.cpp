#include "SceneExportCollectTextures.h"
#include "SceneExportCollect.h"
#include "pure/Model.h"
#include "gltf/GLTFMaterial.h"
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

            const auto &mat = model.materials[mid];

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

            addRef(mat.pbr.baseColorTexture);
            addRef(mat.pbr.metallicRoughnessTexture);
            addRef(mat.normalTexture);
            addRef(mat.occlusionTexture);
            addRef(mat.emissiveTexture);
            if (mat.extTransmission)        addRef(mat.extTransmission->transmissionTexture);
            if (mat.extDiffuseTransmission) addRef(mat.extDiffuseTransmission->diffuseTransmissionTexture);
            if (mat.extSpecular)
            {
                addRef(mat.extSpecular->specularTexture);
                addRef(mat.extSpecular->specularColorTexture);
            }
            if (mat.extClearcoat)
            {
                addRef(mat.extClearcoat->clearcoatTexture);
                addRef(mat.extClearcoat->clearcoatRoughnessTexture);
                addRef(mat.extClearcoat->clearcoatNormalTexture);
            }
            if (mat.extSheen)
            {
                addRef(mat.extSheen->sheenColorTexture);
                addRef(mat.extSheen->sheenRoughnessTexture);
            }
            if (mat.extVolume)      addRef(mat.extVolume->thicknessTexture);
            if (mat.extAnisotropy)  addRef(mat.extAnisotropy->texture);
            if (mat.extIridescence)
            {
                addRef(mat.extIridescence->texture);
                addRef(mat.extIridescence->thicknessTexture);
            }
        }

        outTextures.assign(texSet.begin(), texSet.end());
        outImages.assign(imgSet.begin(), imgSet.end());
        outSamplers.assign(sampSet.begin(), sampSet.end());
    }
}
