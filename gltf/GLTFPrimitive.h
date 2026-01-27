#pragma once

#include <optional>
#include <cstddef>
#include "gltf/GLTFGeometry.h"

struct GLTFPrimitive
{
    GLTFGeometry geometry;
    std::optional<std::size_t> material;
};
