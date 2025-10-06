#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstddef>
#include "math/NodeTransform.h"

struct GLTFNode
{
    std::string name;
    std::optional<std::size_t> mesh;
    std::vector<std::size_t> children;

    NodeTransform transform;
};
