#pragma once
#include "pure/Material.h"
#include <optional>
#include <cstddef>

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

        nlohmann::json toJson() const override
        {
            nlohmann::json j;
            j["name"] = name;
            j["type"] = type;
            // Serialize PBR fields
            j["pbrMetallicRoughness"]["baseColorFactor"] = { pbr.baseColorFactor[0], pbr.baseColorFactor[1], pbr.baseColorFactor[2], pbr.baseColorFactor[3] };
            if (pbr.metallicFactor != 1.0f) j["pbrMetallicRoughness"]["metallicFactor"] = pbr.metallicFactor;
            if (pbr.roughnessFactor != 1.0f) j["pbrMetallicRoughness"]["roughnessFactor"] = pbr.roughnessFactor;
            // Add texture refs if present
            // Note: Implement full serialization as needed
            return j;
        }
    };
}
