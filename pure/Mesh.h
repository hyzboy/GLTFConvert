#pragma once
#include <string>
#include <vector>
#include <cstdint>

#include "math/BoundingVolumes.h"

namespace pure
{
    // Mesh groups multiple primitives
    struct Mesh
    {
        std::string name;
        std::vector<int32_t> primitives; // indices into Model::primitives

        // Mesh-level bounding volumes computed after conversion
        BoundingVolumes bounding_volume;
    };
}
