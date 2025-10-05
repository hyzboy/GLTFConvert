#pragma once

#include <string>
#include <vector>
#include <cstddef>
#include "math/AABB.h"

namespace gltf {

struct GLTFScene {
    std::string name;
    std::vector<std::size_t> nodes; // root node indices
    ::AABB worldAABB; // computed
};

} // namespace gltf
