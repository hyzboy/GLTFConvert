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
    namespace // internal helpers
    {
        struct SceneBuildContext
        {
            // Reachable nodes (old indices, in DFS order)
            std::vector<int32_t> nodeOrder;
            // old -> new compact node index
            std::unordered_map<int32_t,int32_t> nodeRemap;

            // Resource remap tables
            std::unordered_map<int32_t,int32_t> subMeshRemap;              // old subMesh index -> new
            std::unordered_map<std::size_t,std::size_t> matrixRemap;       // old matrix index -> new
            std::unordered_map<std::size_t,std::size_t> trsRemap;          // old trs index -> new
            std::unordered_map<std::size_t,std::size_t> boundsRemap;       // old bounds index -> new
        };

        // Collect reachable nodes starting from scene roots (DFS) and build node remap directly.
        static SceneBuildContext CollectReachable(const Model& sm, const Scene& sc)
        {
            SceneBuildContext ctx;
            ctx.nodeOrder.reserve(sm.mesh_nodes.size());
            ctx.nodeRemap.reserve(sm.mesh_nodes.size());

            std::vector<uint8_t> visited(sm.mesh_nodes.size(), 0);
            std::vector<int32_t> stack;
            stack.reserve(sc.nodes.size());
            for (auto r : sc.nodes)
                stack.push_back(r);

            while (!stack.empty())
            {
                int32_t ni = stack.back();
                stack.pop_back();
                if (ni < 0 || static_cast<std::size_t>(ni) >= sm.mesh_nodes.size())
                    continue;
                if (visited[static_cast<std::size_t>(ni)])
                    continue;
                visited[static_cast<std::size_t>(ni)] = 1;
                int32_t newIndex = static_cast<int32_t>(ctx.nodeOrder.size());
                ctx.nodeOrder.push_back(ni);
                ctx.nodeRemap.emplace(ni, newIndex);
                for (auto c : sm.mesh_nodes[static_cast<std::size_t>(ni)].children)
                    stack.push_back(c);
            }
            return ctx;
        }

        // Scan reachable nodes to build resource remap tables (subMeshes / matrices / trs / bounds)
        static void BuildResourceRemaps(const Model& sm, const Scene& sc, SceneBuildContext& ctx)
        {
            for (auto oldNi : ctx.nodeOrder)
            {
                const auto& n = sm.mesh_nodes[static_cast<std::size_t>(oldNi)];
                for (auto smi : n.subMeshes)
                    ctx.subMeshRemap.try_emplace(smi, static_cast<int32_t>(ctx.subMeshRemap.size()));
                if (n.localMatrixIndexPlusOne)
                    ctx.matrixRemap.try_emplace(n.localMatrixIndexPlusOne - 1, ctx.matrixRemap.size());
                if (n.worldMatrixIndexPlusOne)
                    ctx.matrixRemap.try_emplace(n.worldMatrixIndexPlusOne - 1, ctx.matrixRemap.size());
                if (n.trsIndexPlusOne)
                    ctx.trsRemap.try_emplace(n.trsIndexPlusOne - 1, ctx.trsRemap.size());
                if (n.boundsIndex != kInvalidBoundsIndex)
                    ctx.boundsRemap.try_emplace(n.boundsIndex, ctx.boundsRemap.size());
            }
            if (sc.boundsIndex != kInvalidBoundsIndex)
                ctx.boundsRemap.try_emplace(sc.boundsIndex, ctx.boundsRemap.size());
        }

        // Copy referenced pools using remap tables into the exporter
        static void CopyResourcePools(const Model& sm, const SceneBuildContext& ctx, SceneExporter& out)
        {
            out.subMeshes.resize(ctx.subMeshRemap.size());
            for (const auto& kv : ctx.subMeshRemap)
                out.subMeshes[static_cast<std::size_t>(kv.second)] = sm.subMeshes[static_cast<std::size_t>(kv.first)];

            out.matrixData.resize(ctx.matrixRemap.size());
            for (const auto& kv : ctx.matrixRemap)
                out.matrixData[kv.second] = sm.matrixData[kv.first];

            out.trsPool.resize(ctx.trsRemap.size());
            for (const auto& kv : ctx.trsRemap)
                out.trsPool[kv.second] = sm.trsPool[kv.first];

            out.bounds.resize(ctx.boundsRemap.size());
            for (const auto& kv : ctx.boundsRemap)
                out.bounds[kv.second] = sm.bounds[kv.first];
        }

        // Build scene-local node array with remapped child/subMesh indices and resource indices
        static void BuildNodes(const Model& sm, const SceneBuildContext& ctx, SceneExporter& out)
        {
            out.nodes.reserve(ctx.nodeOrder.size());
            for (auto oldNi : ctx.nodeOrder)
            {
                const auto& on = sm.mesh_nodes[static_cast<std::size_t>(oldNi)];
                MeshNode nn;
                nn.name = on.name;
                nn.boundsIndex = on.boundsIndex;
                if (nn.boundsIndex != kInvalidBoundsIndex)
                    nn.boundsIndex = ctx.boundsRemap.at(static_cast<std::size_t>(nn.boundsIndex));
                if (on.localMatrixIndexPlusOne)
                    nn.localMatrixIndexPlusOne = static_cast<int32_t>(ctx.matrixRemap.at(on.localMatrixIndexPlusOne - 1)) + 1;
                if (on.worldMatrixIndexPlusOne)
                    nn.worldMatrixIndexPlusOne = static_cast<int32_t>(ctx.matrixRemap.at(on.worldMatrixIndexPlusOne - 1)) + 1;
                if (on.trsIndexPlusOne)
                    nn.trsIndexPlusOne = static_cast<int32_t>(ctx.trsRemap.at(on.trsIndexPlusOne - 1)) + 1;
                nn.children.reserve(on.children.size());
                for (auto oc : on.children)
                {
                    auto it = ctx.nodeRemap.find(oc);
                    if (it != ctx.nodeRemap.end())
                        nn.children.push_back(it->second);
                }
                nn.subMeshes.reserve(on.subMeshes.size());
                for (auto osm : on.subMeshes)
                {
                    auto it = ctx.subMeshRemap.find(osm);
                    if (it != ctx.subMeshRemap.end())
                        nn.subMeshes.push_back(it->second);
                }
                out.nodes.push_back(std::move(nn));
            }
        }

        // Build roots array & scene bounds reference
        static void BuildRootsAndSceneBounds(const Scene& sc, const SceneBuildContext& ctx, SceneExporter& out)
        {
            out.roots.clear();
            out.roots.reserve(sc.nodes.size());
            for (auto r : sc.nodes)
            {
                auto it = ctx.nodeRemap.find(r);
                if (it != ctx.nodeRemap.end())
                    out.roots.push_back(it->second);
            }
            out.sceneBoundsIndex = (sc.boundsIndex != kInvalidBoundsIndex)
                ? static_cast<int32_t>(ctx.boundsRemap.at(sc.boundsIndex))
                : static_cast<int32_t>(kInvalidBoundsIndex);
        }

        // Build name table (scene name, subMesh geometry file names, node names)
        static void BuildNameTable(const Model& sm, SceneExporter& out)
        {
            out.nameList.clear();
            out.nameMap.clear();
            out.subMeshNameIndices.clear();
            out.nodeNameIndices.clear();
            std::string baseName = std::filesystem::path(sm.gltf_source).stem().string();
            auto intern_name = [&](const std::string& s) -> uint32_t
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
            for (const auto& smSub : out.subMeshes)
            {
                std::string geomFile = baseName + "." + std::to_string(smSub.geometry);
                out.subMeshNameIndices.push_back(intern_name(geomFile));
            }
            out.nodeNameIndices.reserve(out.nodes.size());
            for (const auto& n : out.nodes)
                out.nodeNameIndices.push_back(intern_name(n.name));
        }
    } // anonymous namespace

    SceneExporter SceneExporter::Build(const Model &sm,int32_t sceneIndex)
    {
        SceneExporter out;
        if (sceneIndex >= static_cast<int32_t>(sm.scenes.size()))
            return out;

        const auto &sc = sm.scenes[sceneIndex];
        out.name = sc.name;

        // 1. Collect reachable nodes & node remap
        SceneBuildContext ctx = CollectReachable(sm, sc);
        // 2. Build resource remaps
        BuildResourceRemaps(sm, sc, ctx);
        // 3. Copy referenced resource pools
        CopyResourcePools(sm, ctx, out);
        // 4. Build nodes
        BuildNodes(sm, ctx, out);
        // 5. Roots & scene bounds
        BuildRootsAndSceneBounds(sc, ctx, out);
        // 6. Name table
        BuildNameTable(sm, out);

        return out;
    }
} // namespace pure
