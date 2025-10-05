#pragma once

#include <cstdint>

namespace pure {

struct MatrixEntry {
    // Indices (plus one) into Model::matrixData (0 means identity/not stored)
    int32_t localIndexPlusOne{0};
    int32_t worldIndexPlusOne{0};
};

} // namespace pure
