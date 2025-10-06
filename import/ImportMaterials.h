#pragma once

#include <fastgltf/core.hpp>
#include "gltf/Model.h"

namespace importers {
    void ImportMaterials(const fastgltf::Asset& asset, GLTFModel& outModel);
} // namespace importers
