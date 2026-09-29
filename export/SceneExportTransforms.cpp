#include "SceneExportTransforms.h"

#include "pure/Model.h"
#include "pure/Scene.h"
#include "pure/Node.h"

#include <functional>

namespace exporters
{
    int32_t GetOrAddTRS(std::vector<TRS> &table, const TRS &t)
    {
        for (int32_t i = 0; i < static_cast<int32_t>(table.size()); ++i)
            if (table[i] == t) return i;
        table.push_back(t);
        return static_cast<int32_t>(table.size()) - 1;
    }

    // 仅导出内部使用（world AABB）；产物只存 TRS
    std::vector<glm::mat4> ComputeWorldMatrices(const pure::Model &model, const pure::Scene &scene)
    {
        std::vector<glm::mat4> world(model.nodes.size(), glm::mat4(1.0f));
        std::function<void(int32_t, const glm::mat4&)> rec = [&](int32_t idx, const glm::mat4 &parent)
        {
            if (idx < 0 || idx >= static_cast<int32_t>(model.nodes.size())) return;
            const auto &node = model.nodes[idx];
            glm::mat4 W = parent * node.transform.rawMat4();
            world[idx] = W;
            for (int32_t c : node.children) rec(c, W);
        };
        for (int32_t root : scene.nodes) rec(root, glm::mat4(1.0f));
        return world;
    }
}
