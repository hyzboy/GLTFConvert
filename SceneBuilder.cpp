#include "SceneExporter.h"
#include "pure/ModelCore.h"
#include "pure/MeshNode.h"
#include "pure/Scene.h"
#include "pure/SubMesh.h"

#include <unordered_map>
#include <cstdint>
#include <filesystem>

namespace pure
{
    namespace
    { // internal helpers

        struct SceneBuildContext
        {
            std::vector<int32_t> nodeOrder;                    // old node indices in traversal order
            std::unordered_map<int32_t,int32_t> nodeRemap;    // old node index -> new node index
            std::unordered_map<int32_t,int32_t> subMeshRemap; // old submesh index -> new submesh index
            std::unordered_map<std::size_t,std::size_t> matrixRemap; // old matrix index -> new index
            std::unordered_map<std::size_t,std::size_t> trsRemap;    // old trs index -> new index
            std::unordered_map<std::size_t,std::size_t> boundsRemap; // old bounds index -> new index (currently unused in remap build)
        };

        static SceneBuildContext CollectReachable(const Model &sm,const Scene &sc)
        {
            SceneBuildContext ctx;
            ctx.nodeOrder.reserve(sm.mesh_nodes.size());
            ctx.nodeRemap.reserve(sm.mesh_nodes.size());

            std::vector<uint8_t> visited(sm.mesh_nodes.size(),0);
            std::vector<int32_t> stack;
            stack.reserve(sc.nodes.size());

            for(auto r:sc.nodes) stack.push_back(r);

            while(!stack.empty())
            {
                int32_t ni=stack.back();
                stack.pop_back();
                if(ni<0||(size_t)ni>=sm.mesh_nodes.size()) continue;
                if(visited[(size_t)ni]) continue;
                visited[(size_t)ni]=1;

                int32_t newIndex=(int32_t)ctx.nodeOrder.size();
                ctx.nodeOrder.push_back(ni);
                ctx.nodeRemap.emplace(ni,newIndex);

                for(auto c:sm.mesh_nodes[(size_t)ni].children) stack.push_back(c);
            }
            return ctx;
        }

        static void BuildResourceRemaps(const Model &sm,const Scene &sc,SceneBuildContext &ctx)
        {
            (void)sc; // sc currently unused here, silence warning if any.
            for(auto oldNi:ctx.nodeOrder)
            {
                const auto &n=sm.mesh_nodes[(size_t)oldNi];
                for(auto smi:n.subMeshes)
                    ctx.subMeshRemap.try_emplace(smi,(int32_t)ctx.subMeshRemap.size());
                if(n.localMatrixIndexPlusOne)
                    ctx.matrixRemap.try_emplace(n.localMatrixIndexPlusOne-1,ctx.matrixRemap.size());
                if(n.worldMatrixIndexPlusOne)
                    ctx.matrixRemap.try_emplace(n.worldMatrixIndexPlusOne-1,ctx.matrixRemap.size());
                if(n.trsIndexPlusOne)
                    ctx.trsRemap.try_emplace(n.trsIndexPlusOne-1,ctx.trsRemap.size());
                // boundsRemap only used if bounds referenced; built later when copying nodes.
            }
        }

        static void CopyResourcePools(const Model &sm,const SceneBuildContext &ctx,SceneExporter &out)
        {
            // SubMeshes
            out.subMeshes.reserve(ctx.subMeshRemap.size());
            std::vector<SubMesh> tmp(sm.subMeshes); // copy to allow index lookups
            for(auto &kv:ctx.subMeshRemap)
            {
                out.subMeshes.push_back(tmp[(size_t)kv.first]);
            }

            // Matrices
            out.matrixData.reserve(ctx.matrixRemap.size());
            for(auto &kv:ctx.matrixRemap)
            {
                out.matrixData.push_back(sm.matrixData[kv.first]);
            }

            // TRS pool
            out.trsPool.reserve(ctx.trsRemap.size());
            for(auto &kv:ctx.trsRemap)
            {
                out.trsPool.push_back(sm.trsPool[kv.first]);
            }

            // Bounds (if any were remapped)
            out.bounds.reserve(ctx.boundsRemap.size());
            for(auto &kv:ctx.boundsRemap)
            {
                out.bounds.push_back(sm.bounds[kv.first]);
            }
        }

        static void BuildNodes(const Model &sm,const SceneBuildContext &ctx,SceneExporter &out)
        {
            for(auto oldNi:ctx.nodeOrder)
            {
                const auto &on=sm.mesh_nodes[(size_t)oldNi];
                MeshNode nn;
                nn.name=on.name;
                nn.boundsIndex=on.boundsIndex;
                if(nn.boundsIndex!=kInvalidBoundsIndex)
                    nn.boundsIndex=(int32_t)ctx.boundsRemap.at((size_t)nn.boundsIndex);
                if(on.localMatrixIndexPlusOne)
                    nn.localMatrixIndexPlusOne=(int32_t)ctx.matrixRemap.at(on.localMatrixIndexPlusOne-1)+1;
                if(on.worldMatrixIndexPlusOne)
                    nn.worldMatrixIndexPlusOne=(int32_t)ctx.matrixRemap.at(on.worldMatrixIndexPlusOne-1)+1;
                if(on.trsIndexPlusOne)
                    nn.trsIndexPlusOne=(int32_t)ctx.trsRemap.at(on.trsIndexPlusOne-1)+1;

                nn.children.reserve(on.children.size());
                for(auto oc:on.children)
                {
                    auto it=ctx.nodeRemap.find(oc);
                    if(it!=ctx.nodeRemap.end()) nn.children.push_back(it->second);
                }

                nn.subMeshes.reserve(on.subMeshes.size());
                for(auto osm:on.subMeshes)
                {
                    auto it=ctx.subMeshRemap.find(osm);
                    if(it!=ctx.subMeshRemap.end()) nn.subMeshes.push_back(it->second);
                }

                out.nodes.push_back(std::move(nn));
            }
        }

        static void BuildRootsAndSceneBounds(const Scene &sc,const SceneBuildContext &ctx,SceneExporter &out)
        {
            out.roots.clear();
            out.roots.reserve(sc.nodes.size());
            for(auto r:sc.nodes)
            {
                auto it=ctx.nodeRemap.find(r);
                if(it!=ctx.nodeRemap.end()) out.roots.push_back(it->second);
            }
            out.sceneBoundsIndex=(sc.boundsIndex!=kInvalidBoundsIndex)
                ?(int32_t)ctx.boundsRemap.at(sc.boundsIndex)
                :(int32_t)kInvalidBoundsIndex;
        }

        static void BuildNameTable(const Model &sm,SceneExporter &out)
        {
            out.nameList.clear();
            out.nameMap.clear();
            out.subMeshNameIndices.clear();
            out.nodeNameIndices.clear();

            std::string baseName=std::filesystem::path(sm.gltf_source).stem().string();

            auto intern_name=[&](const std::string &s) -> uint32_t
                {
                    auto it=out.nameMap.find(s);
                    if(it!=out.nameMap.end()) return it->second;
                    uint32_t idx=(uint32_t)out.nameList.size();
                    out.nameList.push_back(s);
                    out.nameMap.emplace(out.nameList.back(),idx);
                    return idx;
                };

            intern_name(out.name); // ensure scene/exporter name is first (or present)

            out.subMeshNameIndices.reserve(out.subMeshes.size());
            for(const auto &smSub:out.subMeshes)
            {
                std::string geomFile=baseName+"."+std::to_string(smSub.geometry);
                out.subMeshNameIndices.push_back(intern_name(geomFile));
            }

            out.nodeNameIndices.reserve(out.nodes.size());
            for(const auto &n:out.nodes)
                out.nodeNameIndices.push_back(intern_name(n.name));
        }

    } // anonymous namespace

    SceneExporter SceneExporter::Build(const Model &sm,int32_t sceneIndex)
    {
        SceneExporter out;
        if(sceneIndex>=(int32_t)sm.scenes.size()) return out;

        const auto &sc=sm.scenes[(size_t)sceneIndex];
        out.name=sc.name;

        SceneBuildContext ctx=CollectReachable(sm,sc);
        BuildResourceRemaps(sm,sc,ctx);
        CopyResourcePools(sm,ctx,out);
        BuildNodes(sm,ctx,out);
        BuildRootsAndSceneBounds(sc,ctx,out);
        BuildNameTable(sm,out);

        return out;
    }

} // namespace pure
