#pragma once

#include <string>
#include <vector>
#include <cstddef>

#include "Geometry.h" // for kInvalidBoundsIndex

namespace pure {

struct Scene {
    std::string name;
    std::vector<std::size_t> nodes; // root node indices
    // Index into PureModel::bounds pool for this scene's world-space combined bounds
    std::size_t boundsIndex = kInvalidBoundsIndex;
};

} // namespace pure
