#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include <unordered_map>

#include "math/TRS.h"

namespace pure { struct Model; struct Scene; }

namespace exporters
{
    int32_t GetOrAddMatrix(std::vector<glm::mat4> &table,
                           const glm::mat4 &m);

    int32_t GetOrAddTRS(std::vector<TRS> &table,
                        const TRS &t);

    std::vector<glm::mat4> ComputeWorldMatrices(const pure::Model &model,
                                                const pure::Scene &scene);
}
