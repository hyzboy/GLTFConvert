#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include "math/NodeTransform.h"

namespace pure {
    struct Node {
        std::string name;               // node name
        NodeTransform transform;        // local transform (matrix or TRS)
        std::vector<int32_t> children;  // child node indices
        std::optional<int32_t> mesh;    // referenced mesh index (if any)
        std::optional<std::vector<int32_t>> materials; // indices into Model::materials
    };
}
