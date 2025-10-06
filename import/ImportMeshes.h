#pragma once

#include <fastgltf/core.hpp>
#include <vector>
#include "gltf/Mesh.h"

namespace importers {
    // Build mesh list and primitive index mapping; only needs meshes container.
    void ImportMeshes(const fastgltf::Asset& asset, std::vector<GLTFMesh>& meshes);
} // namespace importers
