#pragma once

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <vector>
#include "gltf/Primitive.h"
#include "common/FastGLTFConversions.h"

namespace importers {
    // Import all primitives (geometry + material index) from asset in linear order.
    void ImportPrimitives(const fastgltf::Asset& asset, std::vector<GLTFPrimitive>& primitives);
} // namespace importers
