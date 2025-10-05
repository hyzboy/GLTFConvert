#include "SceneLocal.h"
#include "StaticMesh.h"

#include <algorithm>
#include <unordered_map>
#include <cstdint>

namespace pure {

SceneLocal BuildSceneLocal(const Model& sm, int32_t sceneIndex) {
    SceneLocal out;
    if (sceneIndex >= sm.scenes.size()) return out;
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

    std::unordered_map<std::size_t, std::size_t> subMeshRemap;
    std::unordered_map<std::size_t, std::size_t> matrixRemap;
    std::unordered_map<std::size_t, std::size_t> trsRemap;
    std::unordered_map<std::size_t, std::size_t> boundsRemap;

    for (auto oldNi : nodeOrder) {
        const auto& n = sm.mesh_nodes[static_cast<std::size_t>(oldNi)];
        for (auto smi : n.subMeshes) subMeshRemap.try_emplace(smi, subMeshRemap.size());
        if (n.matrixIndexPlusOne != 0) {
            const auto oldM = n.matrixIndexPlusOne - 1;
            if (matrixRemap.find(oldM) == matrixRemap.end()) matrixRemap.emplace(oldM, matrixRemap.size());
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
    for (const auto& kv : subMeshRemap) out.subMeshes[kv.second] = sm.subMeshes[kv.first];

    out.matrixPool.resize(matrixRemap.size());
    for (const auto& kv : matrixRemap) out.matrixPool[kv.second] = sm.matrixPool[kv.first];

    out.trsPool.resize(trsRemap.size());
    for (const auto& kv : trsRemap) out.trsPool[kv.second] = sm.trsPool[kv.first];

    out.bounds.resize(boundsRemap.size());
    for (const auto& kv : boundsRemap) out.bounds[kv.second] = sm.bounds[kv.first];

    // 4) Local nodes
    out.nodes.reserve(nodeOrder.size());
    for (auto oldNi : nodeOrder) {
        const auto& on = sm.mesh_nodes[static_cast<std::size_t>(oldNi)];
        MeshNode nn = on;
        // remap children
        std::vector<int32_t> newChildren; newChildren.reserve(on.children.size());
        for (auto oc : on.children) {
            if (oc <= static_cast<std::size_t>(std::numeric_limits<int32_t>::max())) {
                auto it = nodeRemap.find(static_cast<int32_t>(oc));
                if (it != nodeRemap.end()) newChildren.push_back(it->second);
            }
        }
        nn.children = std::move(newChildren);
        // remap subMeshes
        std::vector<std::size_t> newSM; newSM.reserve(on.subMeshes.size());
        for (auto osm : on.subMeshes) newSM.push_back(subMeshRemap[osm]);
        nn.subMeshes = std::move(newSM);
        // remap matrix/trs
        if (nn.matrixIndexPlusOne != 0) {
            const auto oldM = nn.matrixIndexPlusOne - 1;
            nn.matrixIndexPlusOne = matrixRemap[oldM] + 1;
        }
        if (nn.trsIndexPlusOne != 0) {
            const auto oldT = nn.trsIndexPlusOne - 1;
            nn.trsIndexPlusOne = trsRemap[oldT] + 1;
        }
        // remap bounds
        if (nn.boundsIndex != kInvalidBoundsIndex) nn.boundsIndex = boundsRemap[nn.boundsIndex];

        out.nodes.push_back(std::move(nn));
    }

    // 5) Roots and scene bounds
    out.roots.clear(); out.roots.reserve(sc.nodes.size());
    for (auto r : sc.nodes) {
        if (r <= static_cast<std::size_t>(std::numeric_limits<int32_t>::max())) {
            auto it = nodeRemap.find(static_cast<int32_t>(r));
            if (it != nodeRemap.end()) out.roots.push_back(it->second);
        }
    }
    out.sceneBoundsIndex = (sc.boundsIndex != kInvalidBoundsIndex) ? boundsRemap[sc.boundsIndex] : kInvalidBoundsIndex;

    return out;
}

} // namespace pure
