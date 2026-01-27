#pragma once

#include <fastgltf/core.hpp>
#include "common/PrimitiveType.h"
#include "common/IndexType.h"
#include "common/VkFormat.h"

// fastgltf -> internal type conversion helpers
PrimitiveType FastGLTFModeToPrimitiveType(fastgltf::PrimitiveType t);
IndexType     FastGLTFComponentTypeToIndexType(fastgltf::ComponentType ct);
VkFormat      FastGLTFAccessorTypeToVkFormat(fastgltf::AccessorType at, fastgltf::ComponentType ct);
