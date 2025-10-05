#include "SceneExporter.h"
#include "StaticMesh.h"

#include <algorithm>
#include <unordered_map>
#include <cstdint>
#include <filesystem>
#include <iostream>

#include "mini_pack_builder.h"

namespace pure
{
    SceneExporter SceneExporter::Build(const Model &sm,int32_t sceneIndex)
    {
        SceneExporter out;

        if (sceneIndex >= static_cast<int32_t>(sm.scenes.size()))
            return out;

        const auto &sc = sm.scenes[sceneIndex];

        out.name       = sc.name;

        std::vector<uint8_t> nodeUsed(sm.mesh_nodes.size(), 0);
        std::vector<int32_t> nodeOrder;
        nodeOrder.reserve(sm.mesh_nodes.size());
        std::vector<int32_t> stack;
        stack.reserve(sc.nodes.size());
        for (auto v : sc.nodes)
            stack.push_back(static_cast<int32_t>(v));

        while (!stack.empty())
        {
            auto ni = stack.back();
            stack.pop_back();
            if (ni < 0 || static_cast<std::size_t>(ni) >= sm.mesh_nodes.size())
                continue;
            if (nodeUsed[static_cast<std::size_t>(ni)])
                continue;
            nodeUsed[static_cast<std::size_t>(ni)] = 1;
            nodeOrder.push_back(ni);
            for (auto c : sm.mesh_nodes[static_cast<std::size_t>(ni)].children)
                stack.push_back(static_cast<int32_t>(c));
        }

        std::sort(nodeOrder.begin(), nodeOrder.end());

        std::unordered_map<int32_t, int32_t> nodeRemap;
        nodeRemap.reserve(nodeOrder.size());
        for (int32_t i = 0; i < static_cast<int32_t>(nodeOrder.size()); ++i)
            nodeRemap[nodeOrder[static_cast<std::size_t>(i)]] = i;

        std::unordered_map<int32_t, int32_t>      subMeshRemap;
        std::unordered_map<std::size_t, std::size_t> matrixRemap;
        std::unordered_map<std::size_t, std::size_t> trsRemap;
        std::unordered_map<std::size_t, std::size_t> boundsRemap;

        for (auto oldNi : nodeOrder)
        {
            const auto &n = sm.mesh_nodes[static_cast<std::size_t>(oldNi)];
            for (auto smi : n.subMeshes)
                subMeshRemap.try_emplace(smi, static_cast<int32_t>(subMeshRemap.size()));
            if (n.localMatrixIndexPlusOne)
                matrixRemap.try_emplace(n.localMatrixIndexPlusOne - 1, matrixRemap.size());
            if (n.worldMatrixIndexPlusOne)
                matrixRemap.try_emplace(n.worldMatrixIndexPlusOne - 1, matrixRemap.size());
            if (n.trsIndexPlusOne)
                trsRemap.try_emplace(n.trsIndexPlusOne - 1, trsRemap.size());
            if (n.boundsIndex != kInvalidBoundsIndex)
                boundsRemap.try_emplace(n.boundsIndex, boundsRemap.size());
        }

        if (sc.boundsIndex != kInvalidBoundsIndex)
            boundsRemap.try_emplace(sc.boundsIndex, boundsRemap.size());

        out.subMeshes.resize(subMeshRemap.size());
        for (const auto &kv : subMeshRemap)
            out.subMeshes[static_cast<std::size_t>(kv.second)] = sm.subMeshes[static_cast<std::size_t>(kv.first)];

        out.matrixData.resize(matrixRemap.size());
        for (const auto &kv : matrixRemap)
            out.matrixData[kv.second] = sm.matrixData[kv.first];

        out.trsPool.resize(trsRemap.size());
        for (const auto &kv : trsRemap)
            out.trsPool[kv.second] = sm.trsPool[kv.first];

        out.bounds.resize(boundsRemap.size());
        for (const auto &kv : boundsRemap)
            out.bounds[kv.second] = sm.bounds[kv.first];

        out.nodes.reserve(nodeOrder.size());
        for (auto oldNi : nodeOrder)
        {
            const auto &on = sm.mesh_nodes[static_cast<std::size_t>(oldNi)];
            MeshNode     nn;
            nn.name       = on.name;
            nn.boundsIndex = on.boundsIndex;
            if (nn.boundsIndex != kInvalidBoundsIndex)
                nn.boundsIndex = boundsRemap[nn.boundsIndex];
            if (on.localMatrixIndexPlusOne)
                nn.localMatrixIndexPlusOne = static_cast<int32_t>(matrixRemap[on.localMatrixIndexPlusOne - 1]) + 1;
            if (on.worldMatrixIndexPlusOne)
                nn.worldMatrixIndexPlusOne = static_cast<int32_t>(matrixRemap[on.worldMatrixIndexPlusOne - 1]) + 1;
            if (on.trsIndexPlusOne)
                nn.trsIndexPlusOne = static_cast<int32_t>(trsRemap[on.trsIndexPlusOne - 1]) + 1;
            for (auto oc : on.children)
            {
                auto it = nodeRemap.find(oc);
                if (it != nodeRemap.end())
                    nn.children.push_back(it->second);
            }
            for (auto osm : on.subMeshes)
            {
                auto it = subMeshRemap.find(osm);
                if (it != subMeshRemap.end())
                    nn.subMeshes.push_back(it->second);
            }
            out.nodes.push_back(std::move(nn));
        }

        out.roots.clear();
        out.roots.reserve(sc.nodes.size());
        for (auto r : sc.nodes)
        {
            auto it = nodeRemap.find(r);
            if (it != nodeRemap.end())
                out.roots.push_back(it->second);
        }
        out.sceneBoundsIndex = (sc.boundsIndex != kInvalidBoundsIndex)
            ? static_cast<int32_t>(boundsRemap[sc.boundsIndex])
            : static_cast<int32_t>(kInvalidBoundsIndex);

        // Name table
        out.nameList.clear();
        out.nameMap.clear();
        out.subMeshNameIndices.clear();
        out.nodeNameIndices.clear();
        std::string baseName = std::filesystem::path(sm.gltf_source).stem().string();
        auto intern_name = [&](const std::string &s) -> uint32_t
        {
            auto it = out.nameMap.find(s);
            if (it != out.nameMap.end())
                return it->second;
            uint32_t idx = static_cast<uint32_t>(out.nameList.size());
            out.nameList.push_back(s);
            out.nameMap.emplace(out.nameList.back(), idx);
            return idx;
        };
        intern_name(out.name);
        out.subMeshNameIndices.reserve(out.subMeshes.size());
        for (const auto &smSub : out.subMeshes)
        {
            std::string geomFile = baseName + "." + std::to_string(smSub.geometry);
            out.subMeshNameIndices.push_back(intern_name(geomFile));
        }
        out.nodeNameIndices.reserve(out.nodes.size());
        for (const auto &n : out.nodes)
            out.nodeNameIndices.push_back(intern_name(n.name));

        return out;
    }
} // namespace pure
