#include "SceneBinary.h"

#include <fstream>
#include <iostream>
#include <vector>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <algorithm>

#include "mini_pack_builder.h"

namespace exporters
{
    namespace
    {
    #pragma pack(push,1)
        struct SceneHeader
        {
            uint32_t version;
            uint32_t rootCount;
            uint32_t matrixCount;
            uint32_t trsCount;
            uint32_t subMeshCount;
            uint32_t nodeCount;
        };
    #pragma pack(pop)

        static inline void AppendU32(std::vector<uint8_t> &buf,uint32_t v) { buf.push_back(static_cast<uint8_t>(v&0xFF)); buf.push_back(static_cast<uint8_t>((v>>8)&0xFF)); buf.push_back(static_cast<uint8_t>((v>>16)&0xFF)); buf.push_back(static_cast<uint8_t>((v>>24)&0xFF)); }
        static inline void AppendF32Vec(std::vector<uint8_t> &buf,const float *f,size_t count) { const uint8_t *p=reinterpret_cast<const uint8_t *>(f); buf.insert(buf.end(),p,p+sizeof(float)*count); }
        static inline void AppendBytes(std::vector<uint8_t> &buf,const void *data,size_t size) { const uint8_t *p=reinterpret_cast<const uint8_t *>(data); buf.insert(buf.end(),p,p+size); }

        // File-scoped globals for intermediate data (initialized/cleared at start of WriteSceneBinary)
        static std::vector<std::string> g_nameList;
        static std::unordered_map<std::string,uint32_t> g_nameMap;
        static std::vector<uint32_t> g_subMeshNameIndices;
        static std::vector<uint32_t> g_nodeNameIndices;

        // Add the header entry
        static bool AddHeaderEntry(MiniPackBuilder &builder,const SceneHeader &sh,std::string &err)
        {
            if(!builder.add_entry_from_buffer("Header",&sh,static_cast<uint32_t>(sizeof(SceneHeader)),err))
            {
                std::cerr<<"[Export] Failed to add SceneHeader entry: "<<err<<"\n";
                return false;
            }
            return true;
        }

        // Build NameTable and produce indices for submeshes and nodes (uses file-scoped globals)
        static bool BuildNameTableAndIndices(
            MiniPackBuilder &builder,
            const std::string &sceneName,
            const std::string &baseName,
            const std::vector<pure::SubMesh> &subMeshes,
            const std::vector<pure::MeshNode> &nodes,
            std::string &err)
        {
            // use globals
            g_nameList.clear();
            g_nameMap.clear();
            g_subMeshNameIndices.clear();
            g_nodeNameIndices.clear();

            auto intern_name=[&](const std::string &s) -> uint32_t
                {
                    auto it=g_nameMap.find(s);
                    if(it!=g_nameMap.end()) return it->second;
                    uint32_t idx=static_cast<uint32_t>(g_nameList.size());
                    g_nameList.push_back(s);
                    g_nameMap.emplace(g_nameList.back(),idx);
                    return idx;
                };

            // Intern scene name
            intern_name(sceneName);
            // Intern submesh filenames and fill global indices
            g_subMeshNameIndices.reserve(subMeshes.size());
            for(const auto &sm:subMeshes)
            {
                std::string geomFile=baseName+"."+std::to_string(sm.geometry);
                g_subMeshNameIndices.push_back(intern_name(geomFile));
            }
            // Intern node names and fill global indices
            g_nodeNameIndices.reserve(nodes.size());
            for(const auto &n:nodes) g_nodeNameIndices.push_back(intern_name(n.name));

            // Serialize NameTable: u32 count, then uint8 lengths, then names + null terminators
            std::vector<uint8_t> nameBuf;
            AppendU32(nameBuf,static_cast<uint32_t>(g_nameList.size()));
            for(const auto &s:g_nameList)
            {
                size_t len=s.size();
                if(len>255)
                {
                    std::cerr<<"[Export] Warning: name too long, truncating length field to 255: "<<s<<"\n";
                    len=255;
                }
                nameBuf.push_back(static_cast<uint8_t>(len));
            }
            for(const auto &s:g_nameList)
            {
                if(!s.empty()) AppendBytes(nameBuf,s.data(),s.size());
                nameBuf.push_back(0);
            }

            if(!builder.add_entry_from_buffer("NameTable",nameBuf.data(),static_cast<uint32_t>(nameBuf.size()),err))
            {
                std::cerr<<"[Export] Failed to add NameTable entry: "<<err<<"\n";
                return false;
            }

            return true;
        }

        static bool AddRootsEntry(MiniPackBuilder &builder,const std::vector<std::size_t> &sceneRootIndices,std::string &err)
        {
            std::vector<uint8_t> rootsBuf;
            rootsBuf.reserve(sceneRootIndices.size()*4);
            for(auto r:sceneRootIndices) AppendU32(rootsBuf,static_cast<uint32_t>(r));
            if(!builder.add_entry_from_buffer("Roots",rootsBuf.data(),static_cast<uint32_t>(rootsBuf.size()),err))
            {
                std::cerr<<"[Export] Failed to add Roots entry: "<<err<<"\n";
                return false;
            }
            return true;
        }

        static bool AddMatricesEntry(MiniPackBuilder &builder,const std::vector<pure::MatrixEntry> &matrices,std::string &err)
        {
            std::vector<uint8_t> matsBuf;
            matsBuf.reserve(matrices.size()*sizeof(glm::mat4)*2);
            for(const auto &m:matrices)
            {
                AppendBytes(matsBuf,&m.local,sizeof(glm::mat4));
                AppendBytes(matsBuf,&m.world,sizeof(glm::mat4));
            }
            if(!builder.add_entry_from_buffer("Matrices",matsBuf.data(),static_cast<uint32_t>(matsBuf.size()),err))
            {
                std::cerr<<"[Export] Failed to add Matrices entry: "<<err<<"\n";
                return false;
            }
            return true;
        }

        static bool AddTRSEntry(MiniPackBuilder &builder,const std::vector<pure::MeshNodeTransform> &trs,std::string &err)
        {
            std::vector<uint8_t> trsBuf;
            trsBuf.reserve(trs.size()*sizeof(float)*10);
            for(const auto &t:trs)
            {
                float vals[10]={ static_cast<float>(t.translation.x),static_cast<float>(t.translation.y),static_cast<float>(t.translation.z),
                    static_cast<float>(t.rotation.x),static_cast<float>(t.rotation.y),static_cast<float>(t.rotation.z),static_cast<float>(t.rotation.w),
                    static_cast<float>(t.scale.x),static_cast<float>(t.scale.y),static_cast<float>(t.scale.z) };
                AppendBytes(trsBuf,vals,sizeof(vals));
            }
            if(!builder.add_entry_from_buffer("TRS",trsBuf.data(),static_cast<uint32_t>(trsBuf.size()),err))
            {
                std::cerr<<"[Export] Failed to add TRS entry: "<<err<<"\n";
                return false;
            }
            return true;
        }

        static bool AddSubMeshesEntry(MiniPackBuilder &builder,std::string &err)
        {
            std::vector<uint8_t> smBuf;
            smBuf.reserve(g_subMeshNameIndices.size()*4);
            for(auto idx:g_subMeshNameIndices) AppendU32(smBuf,idx);
            if(!builder.add_entry_from_buffer("SubMeshes",smBuf.data(),static_cast<uint32_t>(smBuf.size()),err))
            {
                std::cerr<<"[Export] Failed to add SubMeshes entry: "<<err<<"\n";
                return false;
            }
            return true;
        }

        static bool AddNodesEntry(MiniPackBuilder &builder,const std::vector<pure::MeshNode> &nodes,std::string &err)
        {
            std::vector<uint8_t> nodesBuf;
            for(size_t i=0; i<nodes.size(); ++i)
            {
                const auto &n=nodes[i];
                AppendU32(nodesBuf,g_nodeNameIndices[i]);
                AppendU32(nodesBuf,static_cast<uint32_t>(n.matrixIndexPlusOne));
                AppendU32(nodesBuf,static_cast<uint32_t>(n.trsIndexPlusOne));
                AppendU32(nodesBuf,static_cast<uint32_t>(n.children.size()));
                for(auto c:n.children) AppendU32(nodesBuf,static_cast<uint32_t>(c));
                AppendU32(nodesBuf,static_cast<uint32_t>(n.subMeshes.size()));
                for(auto sidx:n.subMeshes) AppendU32(nodesBuf,static_cast<uint32_t>(sidx));
            }
            if(!builder.add_entry_from_buffer("Nodes",nodesBuf.data(),static_cast<uint32_t>(nodesBuf.size()),err))
            {
                std::cerr<<"[Export] Failed to add Nodes entry: "<<err<<"\n";
                return false;
            }
            return true;
        }
    } // anonymous namespace

    bool WriteSceneBinary(
        const std::filesystem::path &sceneDir,
        const std::string &sceneName,
        const std::vector<std::size_t> &sceneRootIndices,
        const std::string &baseName,
        const std::vector<pure::MeshNode> &nodes,
        const std::vector<pure::SubMesh> &subMeshes,
        const std::vector<pure::MatrixEntry> &matrices,
        const std::vector<pure::MeshNodeTransform> &trs)
    {
        // Use sceneName + ".scene" as filename; fallback to "Scene.scene" if empty
        std::string fileName=sceneName.empty()?std::string("Scene.scene"):(sceneName+".scene");
        std::filesystem::path binPath=sceneDir/fileName;

        MiniPackBuilder builder;
        std::string err;

        // Header
        SceneHeader sh{};
        sh.version=1;
        sh.rootCount=static_cast<uint32_t>(sceneRootIndices.size());
        sh.matrixCount=static_cast<uint32_t>(matrices.size());
        sh.trsCount=static_cast<uint32_t>(trs.size());
        sh.subMeshCount=static_cast<uint32_t>(subMeshes.size());
        sh.nodeCount=static_cast<uint32_t>(nodes.size());

        if(!AddHeaderEntry(builder,sh,err)) return false;

        // Name table and indices
        if(!BuildNameTableAndIndices(builder,sceneName,baseName,subMeshes,nodes,err)) return false;

        // Roots
        if(!AddRootsEntry(builder,sceneRootIndices,err)) return false;
        // Matrices
        if(!AddMatricesEntry(builder,matrices,err)) return false;
        // TRS
        if(!AddTRSEntry(builder,trs,err)) return false;
        // SubMeshes
        if(!AddSubMeshesEntry(builder,err)) return false;
        // Nodes
        if(!AddNodesEntry(builder,nodes,err)) return false;

        // Write pack to file
        {
            auto writer=create_file_writer(binPath.string());
            if(!writer)
            {
                std::cerr<<"[Export] Cannot open scene binary for writing: "<<binPath<<"\n";
                return false;
            }
            MiniPackBuildResult result;
            if(!builder.build_pack(writer.get(), /*index_only*/false,result,err))
            {
                std::cerr<<"[Export] Failed to build scene pack: "<<err<<"\n";
                return false;
            }
        }

        std::cout<<"[Export] Saved: "<<binPath<<"\n";
        return true;
    }
} // namespace exporters
