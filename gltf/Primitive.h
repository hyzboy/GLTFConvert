#pragma once

#include <optional>
#include <cstddef>
#include "gltf/Geometry.h" // updated gltf geometry definition

namespace gltf {

struct GLTFPrimitive {
    GLTFGeometry geometry; // geometry payload (import-time)
    std::optional<std::size_t> material; // material index
};

} // namespace gltf
