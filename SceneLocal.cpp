#include "SceneLocal.h"
#include "StaticMesh.h"

#include <algorithm>
#include <unordered_map>
#include <cstdint>

namespace pure {

SceneLocal BuildSceneLocal(const Model& sm, int32_t sceneIndex) {
    SceneLocal out;
    if (sceneIndex >= static_cast<int32_t>(sm.scenes.size())) return out;
    const auto& sc = sm.scenes[sceneIndex];
    out.name = sc.name;

    // 1) Collect reachable nodes from scene roots
    std::vector<uint8_t> nodeUsed(sm.mesh_nodes.size(), 0);
    std::vector<int32_t> nodeOrder; nodeOrder.reserve(sm.mesh_nodes.size());
    std::vector<int32_t> stack; stack.reserve(sc.nodes.size());
    for (auto v : sc.nodes) stack.push_back(static_cast<int32_t>(v));
    while (!stack.empty()) {
        auto ni = stack.back(); stack.pop_back();
        if (ni < 0 || static_cast<std::size_t>(ni) >= sm.mesh_nodes.size()) continue;
        if (nodeUsed[static_cast<std::size_t>(ni)]) continue;
        nodeUsed[static_cast<std::size_t>(ni)] = 1;
        nodeOrder.push_back(ni);
        for (auto c : sm.mesh_nodes[static_cast<std::size_t>(ni)].children) stack.push_back(static_cast<int32_t>(c));
    }
    std::sort(nodeOrder.begin(), nodeOrder.end());

    // 2) Build remap tables
    std::unordered_map<int32_t, int32_t> nodeRemap; nodeRemap.reserve(nodeOrder.size());
    for (int32_t i = 0; i < static_cast<int32_t>(nodeOrder.size()); ++i) nodeRemap[nodeOrder[static_cast<std::size_t>(i)]] = i;

    std::unordered_map<int32_t, int32_t> subMeshRemap;
    std::unordered_map<std::size_t, std::size_t> matrixEntryRemap;
    std::unordered_map<std::size_t, std::size_t> matrixDataRemap; // per-matrix reuse
    std::unordered_map<std::size_t, std::size_t> trsRemap;
    std::unordered_map<std::size_t, std::size_t> boundsRemap;

    for (auto oldNi : nodeOrder) {
        const auto& n = sm.mesh_nodes[static_cast<std::size_t>(oldNi)];
        for (auto smi : n.subMeshes) subMeshRemap.try_emplace(smi, static_cast<int32_t>(subMeshRemap.size()));
        if (n.matrixIndexPlusOne != 0) {
            const auto oldEntry = n.matrixIndexPlusOne - 1;
            if (matrixEntryRemap.find(oldEntry) == matrixEntryRemap.end()) matrixEntryRemap.emplace(oldEntry, matrixEntryRemap.size());
            // Also mark matrixData indices used by that entry
            const MatrixEntry &me = sm.matrixEntryPool[oldEntry];
            if (me.localIndexPlusOne) matrixDataRemap.try_emplace(me.localIndexPlusOne - 1, matrixDataRemap.size());
            if (me.worldIndexPlusOne) matrixDataRemap.try_emplace(me.worldIndexPlusOne - 1, matrixDataRemap.size());
        }
        if (n.trsIndexPlusOne != 0) {
            const auto oldT = n.trsIndexPlusOne - 1;
            if (trsRemap.find(oldT) == trsRemap.end()) trsRemap.emplace(oldT, trsRemap.size());
        }
        if (n.boundsIndex != kInvalidBoundsIndex) boundsRemap.try_emplace(n.boundsIndex, boundsRemap.size());
    }
    if (sc.boundsIndex != kInvalidBoundsIndex) boundsRemap.try_emplace(sc.boundsIndex, boundsRemap.size());

    // 3) Build local pools
    out.subMeshes.resize(subMeshRemap.size());
    for (const auto& kv : subMeshRemap) out.subMeshes[static_cast<std::size_t>(kv.second)] = sm.subMeshes[static_cast<std::size_t>(kv.first)];

    out.matrixEntryPool.resize(matrixEntryRemap.size());
    for (const auto& kv : matrixEntryRemap) out.matrixEntryPool[kv.second] = sm.matrixEntryPool[kv.first];

    out.matrixData.resize(matrixDataRemap.size());
    for (const auto& kv : matrixDataRemap) out.matrixData[kv.second] = sm.matrixData[kv.first];

    out.trsPool.resize(trsRemap.size());
    for (const auto& kv : trsRemap) out.trsPool[kv.second] = sm.trsPool[kv.first];

    out.bounds.resize(boundsRemap.size());
    for (const auto& kv : boundsRemap) out.bounds[kv.second] = sm.bounds[kv.first];

    // 4) Local nodes
    out.nodes.reserve(nodeOrder.size());
    for (auto oldNi : nodeOrder) {
        const auto& on = sm.mesh_nodes[static_cast<std::size_t>(oldNi)];
        MeshNode nn; // construct fresh
        nn.name = on.name;
        nn.matrixIndexPlusOne = on.matrixIndexPlusOne;
        nn.trsIndexPlusOne = on.trsIndexPlusOne;
        nn.boundsIndex = on.boundsIndex;

        for (auto oc : on.children) {
            if (oc >= 0) {
                auto it = nodeRemap.find(static_cast<int32_t>(oc));
                if (it != nodeRemap.end()) nn.children.push_back(it->second);
            }
        }
        for (auto osm : on.subMeshes) {
            auto it = subMeshRemap.find(osm);
            if (it != subMeshRemap.end()) nn.subMeshes.push_back(it->second);
        }

        if (nn.matrixIndexPlusOne != 0) {
            const auto oldEntry = nn.matrixIndexPlusOne - 1;
            nn.matrixIndexPlusOne = matrixEntryRemap[oldEntry] + 1;
            // Remap the indices inside the entry pool later after we finish nodes to avoid double iteration
        }
        if (nn.trsIndexPlusOne != 0) {
            const auto oldT = nn.trsIndexPlusOne - 1;
            nn.trsIndexPlusOne = trsRemap[oldT] + 1;
        }
        if (nn.boundsIndex != kInvalidBoundsIndex) nn.boundsIndex = boundsRemap[nn.boundsIndex];

        out.nodes.push_back(std::move(nn));
    }

    // Remap internal indices of matrix entries to point to local matrixData
    for (auto &me : out.matrixEntryPool) {
        if (me.localIndexPlusOne) {
            auto it = matrixDataRemap.find(me.localIndexPlusOne - 1);
            if (it != matrixDataRemap.end()) me.localIndexPlusOne = static_cast<int32_t>(it->second) + 1; else me.localIndexPlusOne = 0;
        }
        if (me.worldIndexPlusOne) {
            auto it = matrixDataRemap.find(me.worldIndexPlusOne - 1);
            if (it != matrixDataRemap.end()) me.worldIndexPlusOne = static_cast<int32_t>(it->second) + 1; else me.worldIndexPlusOne = 0;
        }
    }

    // 5) Roots and scene bounds
    out.roots.clear(); out.roots.reserve(sc.nodes.size());
    for (auto r : sc.nodes) {
        if (r >= 0) {
            auto it = nodeRemap.find(static_cast<int32_t>(r));
            if (it != nodeRemap.end()) out.roots.push_back(it->second);
        }
    }
    out.sceneBoundsIndex = (sc.boundsIndex != kInvalidBoundsIndex) ? static_cast<int32_t>(boundsRemap[sc.boundsIndex]) : static_cast<int32_t>(kInvalidBoundsIndex);

    return out;
}

} // namespace pure
