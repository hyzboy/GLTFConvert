#pragma once

#include <fastgltf/core.hpp>
#include "gltf/Model.h"

namespace importers {
    void ImportMeshes(const fastgltf::Asset& asset, GLTFModel& outModel);
} // namespace importers
