#include "SceneExportData.h"
#include <unordered_set>
#include <unordered_map>
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

        // Helper for inserting a name into table and returning its index (-1 if empty string)
        int32_t GetOrAddName(std::unordered_map<std::string,int32_t> &map,
                              std::vector<std::string> &table,
                              const std::string &name)
        {
            if (name.empty()) return -1;
            auto it = map.find(name);
            if (it != map.end()) return it->second;
            int32_t idx = static_cast<int32_t>(table.size());
            table.push_back(name);
            map.emplace(name, idx);
            return idx;
        }
    } // namespace

    SceneExportData BuildSceneExportData(const pure::Model &model, std::size_t sceneIndex, const std::string &geometryBaseName)
    {
        SceneExportData data;
        if (sceneIndex >= model.scenes.size())
            return data;

        const auto &scene = model.scenes[sceneIndex];

        // 1. Collect raw original indices used by this scene.
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

        // 2. Build stable contiguous remapping tables (original -> scene-local) using deterministic ordering.
        //    We sort the collected indices to have reproducible output instead of hash-set order.
        auto make_sorted_vector = [](const std::unordered_set<int32_t>& src) {
            std::vector<int32_t> v(src.begin(), src.end());
            std::sort(v.begin(), v.end());
            return v;
        };

        std::vector<int32_t> sortedNodes      = make_sorted_vector(nodeSet);
        std::vector<int32_t> sortedSubMeshes  = make_sorted_vector(subMeshSet);
        std::vector<int32_t> sortedMaterials  = make_sorted_vector(materialSet);
        std::vector<int32_t> sortedGeometries = make_sorted_vector(geometrySet);

        std::unordered_map<int32_t,int32_t> nodeRemap;
        std::unordered_map<int32_t,int32_t> subMeshRemap;
        std::unordered_map<int32_t,int32_t> materialRemap;
        std::unordered_map<int32_t,int32_t> geometryRemap;
        nodeRemap.reserve(sortedNodes.size());
        subMeshRemap.reserve(sortedSubMeshes.size());
        materialRemap.reserve(sortedMaterials.size());
        geometryRemap.reserve(sortedGeometries.size());

        for (int32_t i = 0; i < static_cast<int32_t>(sortedNodes.size()); ++i)      nodeRemap[sortedNodes[i]] = i;
        for (int32_t i = 0; i < static_cast<int32_t>(sortedSubMeshes.size()); ++i)  subMeshRemap[sortedSubMeshes[i]] = i;
        for (int32_t i = 0; i < static_cast<int32_t>(sortedMaterials.size()); ++i)  materialRemap[sortedMaterials[i]] = i;
        for (int32_t i = 0; i < static_cast<int32_t>(sortedGeometries.size()); ++i) geometryRemap[sortedGeometries[i]] = i;

        // 3. Build name table (scene + nodes + materials) with deduplication and assign indices.
        std::unordered_map<std::string,int32_t> nameToIndex; // for deduplication
        nameToIndex.reserve(sortedNodes.size() + sortedMaterials.size() + 1);

        data.sceneNameIndex = GetOrAddName(nameToIndex, data.nameTable, scene.name);

        // 4. Populate exported arrays in the remapped sorted order so index == scene-local id.
        data.nodes.reserve(sortedNodes.size());
        for (int32_t originalNode : sortedNodes)
        {
            const auto &srcNode = model.mesh_nodes[originalNode];
            SceneNodeExport ne;
            ne.originalIndex = originalNode;
            ne.nameIndex = GetOrAddName(nameToIndex, data.nameTable, srcNode.name);
            ne.localMatrix = srcNode.node_transform.rawMat4();
            ne.subMeshes.reserve(srcNode.subMeshes.size());
            for (int32_t sm : srcNode.subMeshes)
            {
                auto it = subMeshRemap.find(sm);
                if (it != subMeshRemap.end())
                    ne.subMeshes.push_back(it->second);
            }
            ne.children.reserve(srcNode.children.size());
            for (int32_t c : srcNode.children)
            {
                auto it = nodeRemap.find(c);
                if (it != nodeRemap.end())
                    ne.children.push_back(it->second);
            }
            data.nodes.push_back(std::move(ne));
        }

        data.subMeshes.reserve(sortedSubMeshes.size());
        for (int32_t originalSM : sortedSubMeshes)
        {
            const auto &sm = model.subMeshes[originalSM];
            SceneSubMeshExport se;
            se.originalIndex = originalSM;
            if (sm.geometry >= 0)
            {
                auto gIt = geometryRemap.find(sm.geometry);
                se.geometryIndex = (gIt != geometryRemap.end()) ? gIt->second : -1;
                if (sm.geometry >= 0)
                    se.geometryFile = geometryBaseName + "." + std::to_string(sm.geometry) + ".geometry"; // keep original id in file name
            }
            else
            {
                se.geometryIndex = -1;
            }
            if (sm.material.has_value())
            {
                auto mIt = materialRemap.find(sm.material.value());
                if (mIt != materialRemap.end())
                    se.materialIndex = mIt->second;
            }
            data.subMeshes.push_back(std::move(se));
        }

        data.materials.reserve(sortedMaterials.size());
        for (int32_t originalMat : sortedMaterials)
        {
            const auto &mat = model.materials[originalMat];
            SceneMaterialExport me;
            me.originalIndex = originalMat;
            me.nameIndex = GetOrAddName(nameToIndex, data.nameTable, mat.name);
            data.materials.push_back(std::move(me));
        }

        data.geometries.reserve(sortedGeometries.size());
        for (int32_t originalGeo : sortedGeometries)
        {
            SceneGeometryExport ge;
            ge.originalIndex = originalGeo;
            ge.file = geometryBaseName + "." + std::to_string(originalGeo) + ".geometry"; // keep original id in filename
            data.geometries.push_back(std::move(ge));
        }

        return data;
    }
}
