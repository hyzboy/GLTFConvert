#pragma once

#include <vector>
#include "gltf/Primitive.h"
#include "gltf/Node.h"

namespace importers {
    // Rotate geometry data (positions / normals / tangents / bitangents) and update local AABB from Y-up to Z-up.
    void RotatePrimitivesYUpToZUp(std::vector<GLTFPrimitive>& primitives);
    // Adjust node local transforms from Y-up to Z-up.
    void RotateNodeLocalTransformsYUpToZUp(std::vector<GLTFNode>& nodes);
} // namespace importers
