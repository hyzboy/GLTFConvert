#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace pure {
    // Mesh groups multiple primitives
    struct Mesh {
        std::string name;
        std::vector<int32_t> primitives; // indices into Model::primitives
    };
}
