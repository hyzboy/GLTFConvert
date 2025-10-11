#pragma once
#include "pure/Material.h"
#include "gltf/GLTFMaterial.h"
#include <vector>
#include <cstddef>

namespace gltf
{
    struct GLTFMaterialImpl : public pure::Material
    {
        GLTFMaterial gltfMaterial; // Embedded GLTFMaterial for GLTF-specific fields

        // Indices into Model::textures / images / samplers that this material references
        std::vector<std::size_t> usedTextures;
        std::vector<std::size_t> usedImages;
        std::vector<std::size_t> usedSamplers;

        GLTFMaterialImpl()
        {
            type = "GLTF"; // Set default type for GLTF materials
        }

        nlohmann::json toJson() const override
        {
            // Placeholder implementation; implement full serialization as needed
            nlohmann::json j;
            j["name"] = name;
            j["type"] = type;
            // TODO: Serialize GLTFMaterial fields properly
            return j;
        }
    };
}
