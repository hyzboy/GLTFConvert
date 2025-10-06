#pragma once

#include <fastgltf/core.hpp>
#include "gltf/Model.h"

namespace importers {
    void ImportScenes(const fastgltf::Asset& asset, GLTFModel& outModel);
} // namespace importers
