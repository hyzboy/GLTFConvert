#pragma once

#include <optional>
#include <cstddef>
#include "gltf/Geometry.h"

namespace gltf {

struct Primitive {
    Geometry geometry; // geometry payload
    std::optional<std::size_t> material; // material index
};

} // namespace gltf
