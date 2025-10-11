#pragma once
#include "pure/PBRMaterial.h"
#include "gltf/GLTFMaterial.h"
#include <vector>
#include <cstddef>

namespace gltf
{
    struct GLTFMaterialImpl : public pure::PBRMaterial
    {
        // GLTF-specific extensions
        std::optional<GLTFExtEmissiveStrength> extEmissiveStrength;
        std::optional<GLTFExtUnlit> extUnlit;
        std::optional<GLTFExtIOR> extIOR;
        std::optional<GLTFExtTransmission> extTransmission;
        std::optional<GLTFExtDiffuseTransmission> extDiffuseTransmission;
        std::optional<GLTFExtSpecular> extSpecular;
        std::optional<GLTFExtClearcoat> extClearcoat;
        std::optional<GLTFExtSheen> extSheen;
        std::optional<GLTFExtVolume> extVolume;
        std::optional<GLTFExtAnisotropy> extAnisotropy;
        std::optional<GLTFExtIridescence> extIridescence;

        // Indices into Model::textures / images / samplers that this material references
        std::vector<std::size_t> usedTextures;
        std::vector<std::size_t> usedImages;
        std::vector<std::size_t> usedSamplers;

        GLTFMaterialImpl()
        {
            type = "GLTF"; // Override to indicate GLTF-specific PBR
        }

        nlohmann::json toJson() const override
        {
            // Placeholder implementation; implement full serialization as needed
            nlohmann::json j = pure::PBRMaterial::toJson(); // Call base
            j["type"] = type;
            // TODO: Add extensions serialization
            return j;
        }
    };
}
