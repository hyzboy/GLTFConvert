#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <cstddef>

#include "pure/Image.h"
#include "pure/Texture.h"
#include "pure/Sampler.h"

namespace exporters
{
    // Export filtered images/textures/samplers into a single JSON named: <baseModelName>.<sceneName>.textures
    // sceneName must already be sanitized (no invalid path chars). If empty, a fallback like scene0 should be chosen by caller.
    void ExportTexturesJson(const std::string &gltfSource,
                            const std::vector<pure::Image> &allImages,
                            const std::vector<pure::Texture> &allTextures,
                            const std::vector<pure::Sampler> &allSamplers,
                            const std::vector<std::size_t> *usedImages,
                            const std::vector<std::size_t> *usedTextures,
                            const std::vector<std::size_t> *usedSamplers,
                            const std::filesystem::path &targetDir,
                            const std::string &sceneName);
}
