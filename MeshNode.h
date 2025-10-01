#pragma once

#include <string>
#include <vector>
#include <cstddef>

#include "Geometry.h" // for kInvalidBoundsIndex

namespace pure {

struct MeshNode {
    std::string name;
    std::vector<std::size_t> children;

    // Separate indices into pools (plus one). 0 indicates identity/empty and not stored in pool.
    std::size_t matrixIndexPlusOne = 0; // refers to PureModel::matrixPool
    std::size_t trsIndexPlusOne = 0;    // refers to PureModel::trsPool

    std::vector<std::size_t> subMeshes; // indices into PureModel::subMeshes

    // Index into PureModel::bounds pool for this node's world-space combined bounds
    std::size_t boundsIndex = kInvalidBoundsIndex;
};

} // namespace pure
