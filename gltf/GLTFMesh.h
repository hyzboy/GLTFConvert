#pragma once

#include <string>
#include <vector>
#include <cstddef>

struct GLTFMesh
{
    std::string name;
    std::vector<std::size_t> primitives; // indices into GLTFModel::primitives
};
