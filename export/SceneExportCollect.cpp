#include "SceneExportCollect.h"

#include "pure/Model.h"
#include "pure/Scene.h"
#include "pure/Node.h"
#include "pure/Mesh.h"
#include "pure/Primitive.h"

#include <unordered_set>
#include <algorithm>

namespace exporters
{
    namespace
    {
        void CollectNodesRecursive(const pure::Model &model,
                                   int32_t nodeIndex,
                                   std::unordered_set<int32_t> &nodeSet,
                                   std::unordered_set<int32_t> &primitiveSet)
        {
            if (nodeIndex < 0 || nodeIndex >= static_cast<int32_t>(model.nodes.size()))
                return;
            if (!nodeSet.insert(nodeIndex).second)
                return;
            const auto &node = model.nodes[nodeIndex];

            if (node.mesh && *node.mesh >= 0 && *node.mesh < static_cast<int32_t>(model.meshes.size()))
            {
                const auto &mesh = model.meshes[*node.mesh];
                for (int32_t pm : mesh.primitives)
                    if (pm >= 0 && pm < static_cast<int32_t>(model.primitives.size()))
                        primitiveSet.insert(pm);
            }
            for (int32_t c : node.children)
                CollectNodesRecursive(model, c, nodeSet, primitiveSet);
        }
    }

    CollectedIndices CollectSceneIndices(const pure::Model &model, const pure::Scene &scene)
    {
        std::unordered_set<int32_t> nodeSet;
        std::unordered_set<int32_t> primitiveSet;
        std::unordered_set<int32_t> materialSet;
        std::unordered_set<int32_t> geometrySet;

        for (int32_t n : scene.nodes)
            CollectNodesRecursive(model, n, nodeSet, primitiveSet);

        for (int32_t primIndex : primitiveSet)
        {
            if (primIndex < 0 || primIndex >= static_cast<int32_t>(model.primitives.size()))
                continue;
            const auto &prim = model.primitives[primIndex];
            if (prim.geometry >= 0 && prim.geometry < static_cast<int32_t>(model.geometry.size()))
                geometrySet.insert(prim.geometry);
            if (prim.material.has_value())
            {
                int32_t m = prim.material.value();
                if (m >= 0 && m < static_cast<int32_t>(model.materials.size()))
                    materialSet.insert(m);
            }
        }

        auto make_sorted_vector = [](const std::unordered_set<int32_t> &src)
        {
            std::vector<int32_t> v(src.begin(), src.end());
            std::sort(v.begin(), v.end());
            return v;
        };

        CollectedIndices ci;
        ci.nodes      = make_sorted_vector(nodeSet);
        ci.primitives = make_sorted_vector(primitiveSet);
        ci.materials  = make_sorted_vector(materialSet);
        ci.geometries = make_sorted_vector(geometrySet);
        return ci;
    }

    RemapTables BuildRemapTables(const CollectedIndices &ci)
    {
        RemapTables r;
        r.nodeRemap.reserve(ci.nodes.size());
        r.primitiveRemap.reserve(ci.primitives.size());
        r.materialRemap.reserve(ci.materials.size());
        r.geometryRemap.reserve(ci.geometries.size());

        for (int32_t i = 0; i < static_cast<int32_t>(ci.nodes.size()); ++i)
            r.nodeRemap[ci.nodes[i]] = i;
        for (int32_t i = 0; i < static_cast<int32_t>(ci.primitives.size()); ++i)
            r.primitiveRemap[ci.primitives[i]] = i;
        for (int32_t i = 0; i < static_cast<int32_t>(ci.materials.size()); ++i)
            r.materialRemap[ci.materials[i]] = i;
        for (int32_t i = 0; i < static_cast<int32_t>(ci.geometries.size()); ++i)
            r.geometryRemap[ci.geometries[i]] = i;
        return r;
    }
}
