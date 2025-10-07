#include "SceneExportCollect.h"

#include "pure/Model.h"
#include "pure/Scene.h"
#include "pure/MeshNode.h"
#include "pure/SubMesh.h"

#include <unordered_set>
#include <algorithm>

namespace exporters
{
    namespace
    {
        void CollectNodesRecursive(const pure::Model &model,
                                   int32_t nodeIndex,
                                   std::unordered_set<int32_t> &nodeSet,
                                   std::unordered_set<int32_t> &subMeshSet)
        {
            if (nodeIndex < 0 || nodeIndex >= static_cast<int32_t>(model.mesh_nodes.size()))
                return;
            if (!nodeSet.insert(nodeIndex).second)
                return; // already processed

            const auto &node = model.mesh_nodes[nodeIndex];

            for (int32_t sm : node.subMeshes)
            {
                if (sm >= 0 && sm < static_cast<int32_t>(model.subMeshes.size()))
                    subMeshSet.insert(sm);
            }

            for (int32_t c : node.children)
            {
                CollectNodesRecursive(model, c, nodeSet, subMeshSet);
            }
        }
    } // namespace

    CollectedIndices CollectSceneIndices(const pure::Model &model, const pure::Scene &scene)
    {
        std::unordered_set<int32_t> nodeSet;
        std::unordered_set<int32_t> subMeshSet;
        std::unordered_set<int32_t> materialSet;
        std::unordered_set<int32_t> geometrySet;

        for (int32_t n : scene.nodes)
            CollectNodesRecursive(model, n, nodeSet, subMeshSet);

        for (int32_t smIndex : subMeshSet)
        {
            if (smIndex < 0 || smIndex >= static_cast<int32_t>(model.subMeshes.size()))
                continue;
            const auto &sm = model.subMeshes[smIndex];
            if (sm.geometry >= 0 && sm.geometry < static_cast<int32_t>(model.geometry.size()))
                geometrySet.insert(sm.geometry);
            if (sm.material.has_value())
            {
                int32_t m = sm.material.value();
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
        ci.nodes       = make_sorted_vector(nodeSet);
        ci.subMeshes   = make_sorted_vector(subMeshSet);
        ci.materials   = make_sorted_vector(materialSet);
        ci.geometries  = make_sorted_vector(geometrySet);
        return ci;
    }

    RemapTables BuildRemapTables(const CollectedIndices &ci)
    {
        RemapTables r;
        r.nodeRemap.reserve(ci.nodes.size());
        r.subMeshRemap.reserve(ci.subMeshes.size());
        r.materialRemap.reserve(ci.materials.size());
        r.geometryRemap.reserve(ci.geometries.size());

        for (int32_t i = 0; i < static_cast<int32_t>(ci.nodes.size()); ++i)
            r.nodeRemap[ci.nodes[i]] = i;
        for (int32_t i = 0; i < static_cast<int32_t>(ci.subMeshes.size()); ++i)
            r.subMeshRemap[ci.subMeshes[i]] = i;
        for (int32_t i = 0; i < static_cast<int32_t>(ci.materials.size()); ++i)
            r.materialRemap[ci.materials[i]] = i;
        for (int32_t i = 0; i < static_cast<int32_t>(ci.geometries.size()); ++i)
            r.geometryRemap[ci.geometries[i]] = i;

        return r;
    }
}
