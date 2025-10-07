#include "SceneExportBounds.h"

#include "pure/Model.h"
#include "pure/MeshNode.h"
#include "pure/SubMesh.h"

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
        if (nodeIndex < 0 || nodeIndex >= static_cast<int32_t>(model.mesh_nodes.size()))
            return;

        const auto &node = model.mesh_nodes[nodeIndex];
        const glm::mat4 &W = worldMatrices[nodeIndex];

        for (int32_t sm : node.subMeshes)
        {
            if (sm < 0 || sm >= static_cast<int32_t>(model.subMeshes.size()))
                continue;
            const auto &sub = model.subMeshes[sm];
            if (sub.geometry < 0 || sub.geometry >= static_cast<int32_t>(model.geometry.size()))
                continue;
            const auto &geo = model.geometry[sub.geometry];
            if (geo.positions.has_value())
            {
                const auto &pos = geo.positions.value();
                outPts.reserve(outPts.size() + pos.size());
                for (const auto &p : pos)
                    outPts.emplace_back(glm::vec3(W * glm::vec4(p, 1.0f)));
            }
        }

        for (int32_t c : node.children)
            GatherWorldPoints(model, c, worldMatrices, outPts);
    }
}
