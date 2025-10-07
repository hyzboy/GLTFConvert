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
        }

        static void BuildNodes(const Model &sm,const SceneBuildContext &ctx,SceneExporter &out)
        {
            for(auto oldNi:ctx.nodeOrder)
            {
                const auto &on=sm.mesh_nodes[(size_t)oldNi];
                MeshNode nn;
                nn.name=on.name;

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
        BuildNameTable(sm,out);

        return out;
    }

} // namespace pure
