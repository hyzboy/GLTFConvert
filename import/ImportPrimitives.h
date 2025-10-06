#pragma once

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include "gltf/Model.h"
#include "common/FastGLTFConversions.h"

namespace importers {
    void ImportPrimitives(const fastgltf::Asset& asset, GLTFModel& outModel);
} // namespace importers
