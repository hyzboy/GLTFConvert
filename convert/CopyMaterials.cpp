#include <vector>
#include <unordered_set>

#include "pure/Material.h"
#include "gltf/GLTFMaterial.h"
#include "pure/Model.h"

namespace pure
{
    // Helper to add texture ref and collect image / sampler indices
    namespace {
        template<typename TSet>
        void AddTextureIndex(const Model &model, std::optional<std::size_t> texIdx, TSet &texSet, TSet &imgSet, TSet &sampSet)
        {
            if (!texIdx) return;
            if (*texIdx >= model.textures.size()) return;
            if (!texSet.insert(*texIdx).second) return; // already
            const auto &tex = model.textures[*texIdx];
            if (tex.image && *tex.image < model.images.size()) imgSet.insert(*tex.image);
            if (tex.sampler && *tex.sampler < model.samplers.size()) sampSet.insert(*tex.sampler);
        }
        template<typename TSet>
        void AddRef(const Model &model, const GLTFTextureRef &ref, TSet &texSet, TSet &imgSet, TSet &sampSet)
        {
            if (!ref.index) return; AddTextureIndex(model, ref.index, texSet, imgSet, sampSet);
        }
    }

    // Now takes model to do collection directly.
    void CopyMaterials(std::vector<Material> &dstMaterials,
                       const std::vector<GLTFMaterial> &srcMaterials,
                       const Model &model)
    {
        dstMaterials.clear();
        dstMaterials.reserve(srcMaterials.size());
        for (const auto &m : srcMaterials)
        {
            Material pm; static_cast<GLTFMaterial&>(pm) = m;

            std::unordered_set<std::size_t> texSet, imgSet, sampSet;
            AddRef(model, m.pbr.baseColorTexture, texSet, imgSet, sampSet);
            AddRef(model, m.pbr.metallicRoughnessTexture, texSet, imgSet, sampSet);
            AddRef(model, m.normalTexture, texSet, imgSet, sampSet);
            AddRef(model, m.occlusionTexture, texSet, imgSet, sampSet);
            AddRef(model, m.emissiveTexture, texSet, imgSet, sampSet);
            if (m.extTransmission)        AddRef(model, m.extTransmission->transmissionTexture, texSet, imgSet, sampSet);
            if (m.extDiffuseTransmission) AddRef(model, m.extDiffuseTransmission->diffuseTransmissionTexture, texSet, imgSet, sampSet);
            if (m.extSpecular)
            {
                AddRef(model, m.extSpecular->specularTexture, texSet, imgSet, sampSet);
                AddRef(model, m.extSpecular->specularColorTexture, texSet, imgSet, sampSet);
            }
            if (m.extClearcoat)
            {
                AddRef(model, m.extClearcoat->clearcoatTexture, texSet, imgSet, sampSet);
                AddRef(model, m.extClearcoat->clearcoatRoughnessTexture, texSet, imgSet, sampSet);
                AddRef(model, m.extClearcoat->clearcoatNormalTexture, texSet, imgSet, sampSet);
            }
            if (m.extSheen)
            {
                AddRef(model, m.extSheen->sheenColorTexture, texSet, imgSet, sampSet);
                AddRef(model, m.extSheen->sheenRoughnessTexture, texSet, imgSet, sampSet);
            }
            if (m.extVolume)      AddRef(model, m.extVolume->thicknessTexture, texSet, imgSet, sampSet);
            if (m.extAnisotropy)  AddRef(model, m.extAnisotropy->texture, texSet, imgSet, sampSet);
            if (m.extIridescence)
            {
                AddRef(model, m.extIridescence->texture, texSet, imgSet, sampSet);
                AddRef(model, m.extIridescence->thicknessTexture, texSet, imgSet, sampSet);
            }

            pm.usedTextures.assign(texSet.begin(), texSet.end());
            pm.usedImages.assign(imgSet.begin(), imgSet.end());
            pm.usedSamplers.assign(sampSet.begin(), sampSet.end());

            dstMaterials.emplace_back(std::move(pm));
        }
    }
} // namespace pure
