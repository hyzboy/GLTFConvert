#pragma once

#include <fastgltf/core.hpp>
#include <vector>
#include "gltf/Scene.h"

namespace importers {
    void ImportScenes(const fastgltf::Asset& asset, std::vector<GLTFScene>& scenes);
} // namespace importers
