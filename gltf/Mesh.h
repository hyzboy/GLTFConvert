#pragma once

#include <string>
#include <vector>
#include <cstddef>

namespace gltf {

struct Mesh {
    std::string name;
    std::vector<std::size_t> primitives; // indices into Model::primitives
};

} // namespace gltf
