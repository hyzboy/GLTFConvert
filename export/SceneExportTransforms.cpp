#include "SceneExportTransforms.h"

#include "pure/Model.h"
#include "pure/Scene.h"
#include "pure/Node.h"

#include <functional>

namespace exporters
{
    int32_t GetOrAddMatrix(std::vector<glm::mat4> &table, const glm::mat4 &m)
    {
        for (int32_t i = 0; i < static_cast<int32_t>(table.size()); ++i)
        {
            bool same = true;
            for (int c = 0; c < 4 && same; ++c)
                for (int r = 0; r < 4; ++r)
                    if (table[i][c][r] != m[c][r]) { same = false; break; }
            if (same) return i;
        }
        table.push_back(m);
        return static_cast<int32_t>(table.size()) - 1;
    }

    int32_t GetOrAddTRS(std::vector<TRS> &table, const TRS &t)
    {
        for (int32_t i = 0; i < static_cast<int32_t>(table.size()); ++i)
            if (table[i] == t) return i;
        table.push_back(t);
        return static_cast<int32_t>(table.size()) - 1;
    }

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
