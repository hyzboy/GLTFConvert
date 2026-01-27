#pragma once
#include <optional>

namespace pure
{
    enum class WrapMode
    {
        ClampToEdge,
        MirroredRepeat,
        Repeat
    };

    enum class FilterMode
    {
        Nearest,
        Linear,
        NearestMipmapNearest,
        LinearMipmapNearest,
        NearestMipmapLinear,
        LinearMipmapLinear
    };

    struct Sampler
    {
        WrapMode wrapS = WrapMode::Repeat;
        WrapMode wrapT = WrapMode::Repeat;
        std::optional<FilterMode> magFilter;
        std::optional<FilterMode> minFilter;
    };
}
