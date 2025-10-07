#include "SceneExportData.h"
#include <unordered_set>
#include "pure/Model.h"
#include "pure/Scene.h"
#include "pure/MeshNode.h"
#include "pure/SubMesh.h"
#include "pure/Material.h"

namespace exporters
{
    namespace
    {
        void CollectNodes(const pure::Model &model,
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
                CollectNodes(model, c, nodeSet, subMeshSet);
            }
        }
    } // namespace

    SceneExportData BuildSceneExportData(const pure::Model &model, std::size_t sceneIndex, const std::string &geometryBaseName)
    {
        SceneExportData data;
        if (sceneIndex >= model.scenes.size())
            return data;

        const auto &scene = model.scenes[sceneIndex];
        data.sceneName = scene.name;

        std::unordered_set<int32_t> nodeSet;
        std::unordered_set<int32_t> subMeshSet;
        std::unordered_set<int32_t> materialSet;
        std::unordered_set<int32_t> geometrySet;

        for (int32_t n : scene.nodes)
        {
            CollectNodes(model, n, nodeSet, subMeshSet);
        }

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

        data.nodes.reserve(nodeSet.size());
        for (int32_t n : nodeSet)
        {
            const auto &node = model.mesh_nodes[n];
            SceneNodeExport ne;
            ne.originalIndex = n;
            ne.name = node.name;
            ne.localMatrix = node.node_transform.rawMat4();
            ne.subMeshes = node.subMeshes;
            ne.children = node.children;
            data.nodes.push_back(std::move(ne));
        }

        data.subMeshes.reserve(subMeshSet.size());
        for (int32_t smIndex : subMeshSet)
        {
            const auto &sm = model.subMeshes[smIndex];
            SceneSubMeshExport se;
            se.originalIndex = smIndex;
            se.geometryIndex = sm.geometry;
            if (sm.geometry >= 0)
                se.geometryFile = geometryBaseName + "." + std::to_string(sm.geometry) + ".geometry";
            se.materialIndex = sm.material;
            data.subMeshes.push_back(std::move(se));
        }

        data.materials.reserve(materialSet.size());
        for (int32_t m : materialSet)
        {
            const auto &mat = model.materials[m];
            SceneMaterialExport me;
            me.originalIndex = m;
            me.name = mat.name;
            data.materials.push_back(std::move(me));
        }

        data.geometries.reserve(geometrySet.size());
        for (int32_t g : geometrySet)
        {
            SceneGeometryExport ge;
            ge.originalIndex = g;
            ge.file = geometryBaseName + "." + std::to_string(g) + ".geometry";
            data.geometries.push_back(std::move(ge));
        }

        return data;
    }
}
