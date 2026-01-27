#include "SceneExportBounds.h"

#include "pure/Model.h"
#include "pure/Node.h"
#include "pure/Mesh.h"
#include "pure/Primitive.h" // now defines Primitive

namespace exporters
{
    int32_t AddBounds(std::vector<BoundingVolumes> &table,
                      const std::vector<glm::vec3> &pts)
    {
        if (pts.empty())
            return -1;
        BoundingVolumes bv;
        bv.fromPoints(pts);
        table.push_back(std::move(bv));
        return static_cast<int32_t>(table.size()) - 1;
    }

    void GatherWorldPoints(const pure::Model &model,
                           int32_t nodeIndex,
                           const std::vector<glm::mat4> &worldMatrices,
                           std::vector<glm::vec3> &outPts)
    {
        if (nodeIndex < 0 || nodeIndex >= static_cast<int32_t>(model.nodes.size()))
            return;

        const auto &node = model.nodes[nodeIndex];
        const glm::mat4 &W = worldMatrices[nodeIndex];

        if (node.mesh && *node.mesh >= 0 && *node.mesh < static_cast<int32_t>(model.meshes.size()))
        {
            const auto &mesh = model.meshes[*node.mesh];
            for (int32_t primIndex : mesh.primitives)
            {
                if (primIndex < 0 || primIndex >= static_cast<int32_t>(model.primitives.size()))
                    continue;
                const auto &prim = model.primitives[primIndex];
                if (prim.geometry < 0 || prim.geometry >= static_cast<int32_t>(model.geometry.size()))
                    continue;
                const auto &geo = model.geometry[prim.geometry];
                if (geo.positions.has_value())
                {
                    const auto &pos = geo.positions.value();
                    outPts.reserve(outPts.size() + pos.size());
                    for (const auto &p : pos)
                        outPts.emplace_back(glm::vec3(W * glm::vec4(p, 1.0f)));
                }
            }
        }

        for (int32_t c : node.children)
            GatherWorldPoints(model, c, worldMatrices, outPts);
    }
}
