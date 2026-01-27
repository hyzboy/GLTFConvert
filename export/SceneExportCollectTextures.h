#pragma once
#include <vector>
#include <cstddef>
namespace pure { struct Model; }
namespace exporters
{
    struct CollectedIndices; // forward from SceneExportCollect.h
    // Collect texture, image, sampler indices referenced by collected.materials set.
    void CollectUsedTextures(const pure::Model &model,
                             const CollectedIndices &ci,
                             std::vector<std::size_t> &outTextures,
                             std::vector<std::size_t> &outImages,
                             std::vector<std::size_t> &outSamplers);
}
