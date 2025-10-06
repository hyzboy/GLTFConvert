#pragma once

#include "gltf/Model.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace importers {
    void ApplyYUpToZUp(GLTFModel& model);
    void ApplyYUpToZUpNodeTransforms(GLTFModel& model);
} // namespace importers
