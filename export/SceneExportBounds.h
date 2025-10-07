#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <cstdint>

#include "math/BoundingVolumes.h"

namespace pure { struct Model; }

namespace exporters
{
    int32_t AddBounds(std::vector<BoundingVolumes> &table,
                      const std::vector<glm::vec3> &pts);

    void GatherWorldPoints(const pure::Model &model,
                           int32_t nodeIndex,
                           const std::vector<glm::mat4> &worldMatrices,
                           std::vector<glm::vec3> &outPts);
}
