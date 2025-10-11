#pragma once
#include <optional>
#include <cstddef>

namespace pure
{
    struct Texture
    {
        std::optional<std::size_t> sampler; // index into samplers
        std::size_t image; // index into images
    };
}
