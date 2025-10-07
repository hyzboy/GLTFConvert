#include <fastgltf/core.hpp>
#include <vector>
#include "gltf/GLTFMaterial.h"

namespace gltf
{
    template<typename TexInfo>
    static void FillTexture(GLTFTextureRef &dst, const TexInfo &src)
    {
        if constexpr (requires { src.textureIndex; }) dst.index = static_cast<std::size_t>(src.textureIndex);
        if constexpr (requires { src.scale; }) dst.scale = src.scale;
        if constexpr (requires { src.strength; }) dst.strength = src.strength;
        if constexpr (requires { src.texCoord; }) dst.texCoord = static_cast<int>(src.texCoord);
    }

    void ImportMaterials(const fastgltf::Asset &asset,std::vector<GLTFMaterial> &materials)
    {
        materials.clear();
        materials.reserve(asset.materials.size());
        for(const auto &m:asset.materials)
        {
            GLTFMaterial om{};
            if(!m.name.empty()) om.name.assign(m.name.begin(),m.name.end());

            // Core PBR
            const auto &p = m.pbrData;
            for(int i=0;i<4;++i) om.pbr.baseColorFactor[i]=p.baseColorFactor[i];
            om.pbr.metallicFactor = p.metallicFactor;
            om.pbr.roughnessFactor = p.roughnessFactor;
            if (p.baseColorTexture) FillTexture(om.pbr.baseColorTexture, *p.baseColorTexture);
            if (p.metallicRoughnessTexture) FillTexture(om.pbr.metallicRoughnessTexture, *p.metallicRoughnessTexture);

            if (m.normalTexture) FillTexture(om.normalTexture, *m.normalTexture);
            if (m.occlusionTexture) FillTexture(om.occlusionTexture, *m.occlusionTexture);
            if (m.emissiveTexture) FillTexture(om.emissiveTexture, *m.emissiveTexture);
            for(int i=0;i<3;++i) om.emissiveFactor[i]=m.emissiveFactor[i];

            switch (m.alphaMode) {
            case fastgltf::AlphaMode::Opaque: om.alphaMode = GLTFMaterial::AlphaMode::Opaque; break;
            case fastgltf::AlphaMode::Mask: om.alphaMode = GLTFMaterial::AlphaMode::Mask; break;
            case fastgltf::AlphaMode::Blend: om.alphaMode = GLTFMaterial::AlphaMode::Blend; break; }
            om.alphaCutoff = m.alphaCutoff;
            om.doubleSided = m.doubleSided;

            if (m.emissiveStrength)
                om.extEmissiveStrength = GLTFExtEmissiveStrength{ m.emissiveStrength };
            if (m.unlit)
                om.extUnlit = GLTFExtUnlit{};
            if (m.ior)
                om.extIOR = GLTFExtIOR{ m.ior };

            if (m.transmission)
            {
                GLTFExtTransmission ext{};
                ext.transmissionFactor = m.transmission->transmissionFactor;
                if (m.transmission->transmissionTexture) FillTexture(ext.transmissionTexture, *m.transmission->transmissionTexture);
                om.extTransmission = std::move(ext);
            }
            if (m.diffuseTransmission)
            {
                GLTFExtDiffuseTransmission ext{};
                ext.diffuseTransmissionFactor = m.diffuseTransmission->diffuseTransmissionFactor;
                if (m.diffuseTransmission->diffuseTransmissionTexture) FillTexture(ext.diffuseTransmissionTexture, *m.diffuseTransmission->diffuseTransmissionTexture);
                om.extDiffuseTransmission = std::move(ext);
            }
            if (m.specular)
            {
                GLTFExtSpecular ext{};
                ext.specularFactor = m.specular->specularFactor;
                for(int i=0;i<3;++i) ext.specularColorFactor[i]=m.specular->specularColorFactor[i];
                if (m.specular->specularTexture) FillTexture(ext.specularTexture, *m.specular->specularTexture);
                if (m.specular->specularColorTexture) FillTexture(ext.specularColorTexture, *m.specular->specularColorTexture);
                om.extSpecular = std::move(ext);
            }
            if (m.clearcoat)
            {
                GLTFExtClearcoat ext{};
                ext.clearcoatFactor = m.clearcoat->clearcoatFactor;
                ext.clearcoatRoughnessFactor = m.clearcoat->clearcoatRoughnessFactor;
                if (m.clearcoat->clearcoatTexture) FillTexture(ext.clearcoatTexture, *m.clearcoat->clearcoatTexture);
                if (m.clearcoat->clearcoatRoughnessTexture) FillTexture(ext.clearcoatRoughnessTexture, *m.clearcoat->clearcoatRoughnessTexture);
                if (m.clearcoat->clearcoatNormalTexture) FillTexture(ext.clearcoatNormalTexture, *m.clearcoat->clearcoatNormalTexture);
                om.extClearcoat = std::move(ext);
            }
            if (m.sheen)
            {
                GLTFExtSheen ext{};
                for(int i=0;i<3;++i) ext.sheenColorFactor[i]=m.sheen->sheenColorFactor[i];
                ext.sheenRoughnessFactor = m.sheen->sheenRoughnessFactor;
                if (m.sheen->sheenColorTexture) FillTexture(ext.sheenColorTexture, *m.sheen->sheenColorTexture);
                if (m.sheen->sheenRoughnessTexture) FillTexture(ext.sheenRoughnessTexture, *m.sheen->sheenRoughnessTexture);
                om.extSheen = std::move(ext);
            }
            if (m.volume)
            {
                GLTFExtVolume ext{};
                ext.thicknessFactor = m.volume->thicknessFactor;
                if (m.volume->thicknessTexture) FillTexture(ext.thicknessTexture, *m.volume->thicknessTexture);
                for(int i=0;i<3;++i) ext.attenuationColor[i]=m.volume->attenuationColor[i];
                ext.attenuationDistance = m.volume->attenuationDistance;
                om.extVolume = std::move(ext);
            }
            if (m.anisotropy)
            {
                GLTFExtAnisotropy ext{};
                ext.strength = m.anisotropy->anisotropyStrength;
                ext.rotation = m.anisotropy->anisotropyRotation;
                if (m.anisotropy->anisotropyTexture) FillTexture(ext.texture, *m.anisotropy->anisotropyTexture);
                om.extAnisotropy = std::move(ext);
            }
            if (m.iridescence)
            {
                GLTFExtIridescence ext{};
                ext.factor = m.iridescence->iridescenceFactor;
                ext.ior = m.iridescence->iridescenceIor;
                ext.thicknessMinimum = m.iridescence->iridescenceThicknessMinimum;
                ext.thicknessMaximum = m.iridescence->iridescenceThicknessMaximum;
                if (m.iridescence->iridescenceTexture) FillTexture(ext.texture, *m.iridescence->iridescenceTexture);
                if (m.iridescence->iridescenceThicknessTexture) FillTexture(ext.thicknessTexture, *m.iridescence->iridescenceThicknessTexture);
                om.extIridescence = std::move(ext);
            }

            materials.emplace_back(std::move(om));
        }
    }
} // namespace gltf
