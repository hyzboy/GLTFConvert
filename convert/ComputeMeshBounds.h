#pragma once
#include <vector>
#include <cstddef>
#include <filesystem>
#include "math/BoundingVolumes.h"

namespace pure { struct Model; struct Mesh; }

namespace convert
{
    // Compute mesh-level bounding volumes for each mesh in the model using geometry positions
    void ComputeMeshBounds(pure::Model &model);
}
