#include <vector>
#include <unordered_set>
#include <memory>

#include "pure/Material.h"
#include "gltf/GLTFMaterial.h"
#include "gltf/GLTFMaterialImpl.h"
#include "pure/Model.h"

namespace pure
{
    // Helper to add texture ref and collect image / sampler indices
    namespace
    {
        template<typename TSet>
        void AddTextureIndex(const Model &model,std::optional<std::size_t> texIdx,TSet &texSet,TSet &imgSet,TSet &sampSet)
        {
            if(!texIdx) return;
            if(*texIdx>=model.textures.size()) return;
            if(!texSet.insert(*texIdx).second) return; // already
            const auto &tex=model.textures[*texIdx];
            if(tex.image&&*tex.image<model.images.size()) imgSet.insert(*tex.image);
            if(tex.sampler&&*tex.sampler<model.samplers.size()) sampSet.insert(*tex.sampler);
        }
        template<typename TSet>
        void AddRef(const Model &model,const GLTFTextureRef &ref,TSet &texSet,TSet &imgSet,TSet &sampSet)
        {
            if(!ref.index) return; AddTextureIndex(model,ref.index,texSet,imgSet,sampSet);
        }
    }

    // Now takes model to do collection directly.
    void CopyMaterials(std::vector<std::unique_ptr<Material>> &dstMaterials,
                       const std::vector<GLTFMaterial> &srcMaterials,
                       const Model &model)
    {
        dstMaterials.clear();
        dstMaterials.reserve(srcMaterials.size());
        for(const auto &m:srcMaterials)
        {
            auto pm = std::make_unique<gltf::GLTFMaterialImpl>();
            pm->name = m.name;
            // Copy PBR fields to neutral structures
            pm->pbr.baseColorFactor[0] = m.pbr.baseColorFactor[0];
            pm->pbr.baseColorFactor[1] = m.pbr.baseColorFactor[1];
            pm->pbr.baseColorFactor[2] = m.pbr.baseColorFactor[2];
            pm->pbr.baseColorFactor[3] = m.pbr.baseColorFactor[3];
            pm->pbr.metallicFactor = m.pbr.metallicFactor;
            pm->pbr.roughnessFactor = m.pbr.roughnessFactor;
            pm->pbr.baseColorTexture.index = m.pbr.baseColorTexture.index;
            pm->pbr.baseColorTexture.texCoord = m.pbr.baseColorTexture.texCoord;
            pm->pbr.baseColorTexture.scale = m.pbr.baseColorTexture.scale;
            pm->pbr.baseColorTexture.strength = m.pbr.baseColorTexture.strength;
            pm->pbr.metallicRoughnessTexture.index = m.pbr.metallicRoughnessTexture.index;
            pm->pbr.metallicRoughnessTexture.texCoord = m.pbr.metallicRoughnessTexture.texCoord;
            pm->pbr.metallicRoughnessTexture.scale = m.pbr.metallicRoughnessTexture.scale;
            pm->pbr.metallicRoughnessTexture.strength = m.pbr.metallicRoughnessTexture.strength;

            if (m.normalTexture.index) {
                pm->normalTexture = pure::TextureRef{ m.normalTexture.index, m.normalTexture.texCoord, m.normalTexture.scale, m.normalTexture.strength };
            }
            if (m.occlusionTexture.index) {
                pm->occlusionTexture = pure::TextureRef{ m.occlusionTexture.index, m.occlusionTexture.texCoord, m.occlusionTexture.scale, m.occlusionTexture.strength };
            }
            if (m.emissiveTexture.index) {
                pm->emissiveTexture = pure::TextureRef{ m.emissiveTexture.index, m.emissiveTexture.texCoord, m.emissiveTexture.scale, m.emissiveTexture.strength };
            }
            pm->emissiveFactor[0] = m.emissiveFactor[0];
            pm->emissiveFactor[1] = m.emissiveFactor[1];
            pm->emissiveFactor[2] = m.emissiveFactor[2];
            pm->alphaMode = static_cast<pure::AlphaMode>(m.alphaMode);
            pm->alphaCutoff = m.alphaCutoff;
            pm->doubleSided = m.doubleSided;
            // Copy extensions
            pm->extEmissiveStrength = m.extEmissiveStrength;
            pm->extUnlit = m.extUnlit;
            pm->extIOR = m.extIOR;
            pm->extTransmission = m.extTransmission;
            pm->extDiffuseTransmission = m.extDiffuseTransmission;
            pm->extSpecular = m.extSpecular;
            pm->extClearcoat = m.extClearcoat;
            pm->extSheen = m.extSheen;
            pm->extVolume = m.extVolume;
            pm->extAnisotropy = m.extAnisotropy;
            pm->extIridescence = m.extIridescence;

            std::unordered_set<std::size_t> texSet,imgSet,sampSet;
            AddRef(model,m.pbr.baseColorTexture,texSet,imgSet,sampSet);
            AddRef(model,m.pbr.metallicRoughnessTexture,texSet,imgSet,sampSet);
            AddRef(model,m.normalTexture,texSet,imgSet,sampSet);
            AddRef(model,m.occlusionTexture,texSet,imgSet,sampSet);
            AddRef(model,m.emissiveTexture,texSet,imgSet,sampSet);
            if(m.extTransmission)        AddRef(model,m.extTransmission->transmissionTexture,texSet,imgSet,sampSet);
            if(m.extDiffuseTransmission) AddRef(model,m.extDiffuseTransmission->diffuseTransmissionTexture,texSet,imgSet,sampSet);
            if(m.extSpecular)
            {
                AddRef(model,m.extSpecular->specularTexture,texSet,imgSet,sampSet);
                AddRef(model,m.extSpecular->specularColorTexture,texSet,imgSet,sampSet);
            }
            if(m.extClearcoat)
            {
                AddRef(model,m.extClearcoat->clearcoatTexture,texSet,imgSet,sampSet);
                AddRef(model,m.extClearcoat->clearcoatRoughnessTexture,texSet,imgSet,sampSet);
                AddRef(model,m.extClearcoat->clearcoatNormalTexture,texSet,imgSet,sampSet);
            }
            if(m.extSheen)
            {
                AddRef(model,m.extSheen->sheenColorTexture,texSet,imgSet,sampSet);
                AddRef(model,m.extSheen->sheenRoughnessTexture,texSet,imgSet,sampSet);
            }
            if(m.extVolume)      AddRef(model,m.extVolume->thicknessTexture,texSet,imgSet,sampSet);
            if(m.extAnisotropy)  AddRef(model,m.extAnisotropy->texture,texSet,imgSet,sampSet);
            if(m.extIridescence)
            {
                AddRef(model,m.extIridescence->texture,texSet,imgSet,sampSet);
                AddRef(model,m.extIridescence->thicknessTexture,texSet,imgSet,sampSet);
            }

            pm->usedTextures.assign(texSet.begin(),texSet.end());
            pm->usedImages.assign(imgSet.begin(),imgSet.end());
            pm->usedSamplers.assign(sampSet.begin(),sampSet.end());

            dstMaterials.emplace_back(std::move(pm));
        }
    }
} // namespace pure
