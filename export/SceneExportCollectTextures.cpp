#include "SceneExportCollectTextures.h"
#include "SceneExportCollect.h"
#include "pure/Model.h"
#include "pure/PBRMaterial.h"
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

            const pure::PBRMaterial *mat = dynamic_cast<const pure::PBRMaterial*>(model.materials[mid].get());
            if (!mat) continue;

            auto addRef = [&](const pure::TextureRef &ref)
            {
                if (!ref.index) return;
                std::size_t ti = *ref.index;
                if (ti >= model.textures.size()) return;
                if (!texSet.insert(ti).second) return; // already processed

                const auto &tiObj = model.textures[ti];
                if (tiObj.image < model.images.size())
                    imgSet.insert(tiObj.image);
                if (tiObj.sampler && *tiObj.sampler < model.samplers.size())
                    sampSet.insert(*tiObj.sampler);
            };

            addRef(mat->pbr.baseColorTexture);
            addRef(mat->pbr.metallicRoughnessTexture);
            if (mat->normalTexture) addRef(*mat->normalTexture);
            if (mat->occlusionTexture) addRef(*mat->occlusionTexture);
            if (mat->emissiveTexture) addRef(*mat->emissiveTexture);
            // No extensions in PBRMaterial
        }

        outTextures.assign(texSet.begin(), texSet.end());
        outImages.assign(imgSet.begin(), imgSet.end());
        outSamplers.assign(sampSet.begin(), sampSet.end());
    }
}
