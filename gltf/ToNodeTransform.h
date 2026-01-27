#pragma once

#include <variant>
#include "math/NodeTransform.h"
#include "fastgltf/types.hpp"
#include "fastgltf/math.hpp"

// Create a NodeTransform from a fastgltf variant (TRS or matrix) without exposing fastgltf in NodeTransform header.
NodeTransform ToNodeTransform(const std::variant<fastgltf::TRS, fastgltf::math::fmat4x4> &src);
