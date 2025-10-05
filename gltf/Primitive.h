#pragma once

#include <optional>
#include <cstddef>
#include "gltf/Geometry.h"

struct GLTFPrimitive
{
    GLTFGeometry geometry;
    std::optional<std::size_t> material;
};
