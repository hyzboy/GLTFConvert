#pragma once

#include <optional>
#include <cstddef>
#include "gltf/Geometry.h" // gltf geometry definition

namespace gltf {

struct Primitive {
    Geometry geometry; // geometry payload (import-time gltf::Geometry)
    std::optional<std::size_t> material; // material index
};

} // namespace gltf
