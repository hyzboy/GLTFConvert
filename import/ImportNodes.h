#pragma once

#include <fastgltf/core.hpp>
#include <vector>
#include "gltf/Node.h"

namespace importers {
    // Import nodes only (names, mesh index, children, local transform)
    void ImportNodes(const fastgltf::Asset& asset, std::vector<GLTFNode>& nodes);
} // namespace importers
