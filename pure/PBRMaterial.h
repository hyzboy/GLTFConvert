#pragma once
#include "Material.h"
#include <optional>
#include <cstddef>
#include <unordered_map>

namespace pure
{
    struct TextureRef
    {
        std::optional<std::size_t> index; // Texture index
        int texCoord = 0;                 // Used UV channel
        float scale = 1.0f;               // normalScale / clearcoatNormal / anisotropy etc.
        float strength = 1.0f;            // occlusionStrength etc.
    };

    struct PBRMetallicRoughness
    {
        float baseColorFactor[4] = {1.f, 1.f, 1.f, 1.f};
        float metallicFactor = 1.f;
        float roughnessFactor = 1.f;
        TextureRef baseColorTexture;
        TextureRef metallicRoughnessTexture;
    };

    enum class AlphaMode
    {
        Opaque,
        Mask,
        Blend
    };

    struct PBRMaterial : public Material
    {
        PBRMetallicRoughness pbr;
        std::optional<TextureRef> normalTexture;
        std::optional<TextureRef> occlusionTexture;
        std::optional<TextureRef> emissiveTexture;
        float emissiveFactor[3] = {0.0f, 0.0f, 0.0f};
        AlphaMode alphaMode = AlphaMode::Opaque;
        float alphaCutoff = 0.5f;
        bool doubleSided = false;

        // Resource usage indices (for export)
        std::vector<std::size_t> usedTextures;
        std::vector<std::size_t> usedImages;
        std::vector<std::size_t> usedSamplers;

        PBRMaterial()
        {
            type = "PBR";
        }

        nlohmann::json toJson(const Model &model,
                               const std::unordered_map<std::size_t,int32_t> &texRemap,
                               const std::unordered_map<std::size_t,int32_t> &imgRemap,
                               const std::unordered_map<std::size_t,int32_t> &sampRemap,
                               const std::string &baseName) const override;
    };
}
