#pragma once

#include <fastgltf/core.hpp>
#include "gltf/Material.h"
#include <vector>

namespace importers {
    // Import only materials from asset into provided materials vector.
    void ImportMaterials(const fastgltf::Asset& asset, std::vector<GLTFMaterial>& materials);
} // namespace importers
