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
    int32_t GetOrAddTRS(std::vector<TRS> &table,
                        const TRS &t);

    /// 世界矩阵（**仅导出内部使用**：算 world AABB 等；产物里不存矩阵，消费者从 TRS 组合）
    std::vector<glm::mat4> ComputeWorldMatrices(const pure::Model &model,
                                                const pure::Scene &scene);
}
