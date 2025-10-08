#pragma once
#include <vector>
#include <cstddef>
#include "gltf/GLTFMaterial.h"

namespace pure
{
    struct Model; // forward

    // Extended material storing references to globally used resources
    struct Material : public GLTFMaterial
    {
        // Indices into Model::textures / images / samplers that this material references
        std::vector<std::size_t> usedTextures;
        std::vector<std::size_t> usedImages;
        std::vector<std::size_t> usedSamplers;
    };
}
