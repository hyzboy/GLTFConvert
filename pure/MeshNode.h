#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <glm/glm.hpp>
#include "pure/BoundsIndex.h"
#include "math/MeshNodeTransform.h"

namespace pure {
struct MeshNode {
    std::string name;
    std::vector<int32_t> children;
    int32_t localMatrixIndexPlusOne = 0;
    int32_t worldMatrixIndexPlusOne = 0;
    int32_t trsIndexPlusOne = 0;
    std::vector<int32_t> subMeshes;
    int32_t boundsIndex = kInvalidBoundsIndex;
};
}
