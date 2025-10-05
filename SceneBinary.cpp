#include "SceneExporter.h"
#include "pure/ModelCore.h"
#include "pure/MeshNode.h"
#include "pure/Scene.h"
#include "pure/SubMesh.h"

#include <filesystem>
#include <iostream>
#include <vector>
#include <cstdint>

#include "mini_pack_builder.h"

namespace pure
{
    namespace
    {

    #pragma pack(push, 1)
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

        static inline void AppendU32(std::vector<uint8_t> &buf,uint32_t v)
        {
            buf.push_back(static_cast<uint8_t>(v&0xFF));
            buf.push_back(static_cast<uint8_t>((v>>8)&0xFF));
            buf.push_back(static_cast<uint8_t>((v>>16)&0xFF));
            buf.push_back(static_cast<uint8_t>((v>>24)&0xFF));
        }

    } // anonymous namespace

    bool SceneExporter::WriteSceneBinary(const std::filesystem::path &sceneDir,const std::string &baseName) const
    {
        std::string fileName=name.empty()?std::string("Scene.scene"):(name+".scene");
        std::filesystem::path binPath=sceneDir/fileName;

        MiniPackBuilder builder;
        std::string err;

        SceneHeader sh{};
        sh.version=1;
        sh.rootCount=static_cast<uint32_t>(roots.size());
        sh.matrixCount=static_cast<uint32_t>(matrixData.size());
        sh.trsCount=static_cast<uint32_t>(trsPool.size());
        sh.subMeshCount=static_cast<uint32_t>(subMeshes.size());
        sh.nodeCount=static_cast<uint32_t>(nodes.size());

        if(!builder.add_entry_from_buffer("Header",&sh,static_cast<uint32_t>(sizeof(SceneHeader)),err))
        {
            std::cerr<<"[Export] Failed Header: "<<err<<"\n";
            return false;
        }

        write_string_list(&builder,"NameTable",nameList,err);
        if(!err.empty())
        {
            std::cerr<<"[Export] Failed NameTable: "<<err<<"\n";
            return false;
        }

        if(!builder.add_entry_from_array<int32_t>("Roots",roots,err))
        {
            std::cerr<<"[Export] Failed Roots: "<<err<<"\n";
            return false;
        }

        if(!builder.add_entry_from_array<glm::mat4>("Matrices",matrixData,err))
        {
            std::cerr<<"[Export] Failed Matrices: "<<err<<"\n";
            return false;
        }

        // Flatten TRS data (translation xyz, rotation xyzw, scale xyz) per entry => 10 floats
        {
            std::vector<float> flat;
            flat.reserve(trsPool.size()*10);
            for(const auto &t:trsPool)
            {
                flat.push_back(t.translation.x);
                flat.push_back(t.translation.y);
                flat.push_back(t.translation.z);
                flat.push_back(t.rotation.x);
                flat.push_back(t.rotation.y);
                flat.push_back(t.rotation.z);
                flat.push_back(t.rotation.w);
                flat.push_back(t.scale.x);
                flat.push_back(t.scale.y);
                flat.push_back(t.scale.z);
            }
            if(!builder.add_entry_from_array<float>("TRS",flat,err))
            {
                std::cerr<<"[Export] Failed TRS: "<<err<<"\n";
                return false;
            }
        }

        if(!builder.add_entry_from_array<uint32_t>("SubMeshes",subMeshNameIndices,err))
        {
            std::cerr<<"[Export] Failed SubMeshes: "<<err<<"\n";
            return false;
        }

        // Serialize nodes:
        // [nameIdx][localMatrix+1][worldMatrix+1][trsIndex+1][childCount][children...][subMeshCount][subMeshIndices...]
        {
            std::vector<uint8_t> buf;
            buf.reserve(nodes.size()*32);

            for(size_t i=0; i<nodes.size(); ++i)
            {
                const auto &n=nodes[i];
                uint32_t nameIdx=(i<nodeNameIndices.size())?nodeNameIndices[i]:0u;

                AppendU32(buf,nameIdx);
                AppendU32(buf,static_cast<uint32_t>(n.localMatrixIndexPlusOne));
                AppendU32(buf,static_cast<uint32_t>(n.worldMatrixIndexPlusOne));
                AppendU32(buf,static_cast<uint32_t>(n.trsIndexPlusOne));

                AppendU32(buf,static_cast<uint32_t>(n.children.size()));
                for(auto c:n.children)
                {
                    AppendU32(buf,static_cast<uint32_t>(c));
                }

                AppendU32(buf,static_cast<uint32_t>(n.subMeshes.size()));
                for(auto s:n.subMeshes)
                {
                    AppendU32(buf,static_cast<uint32_t>(s));
                }
            }

            if(!builder.add_entry_from_buffer("Nodes",buf.data(),static_cast<uint32_t>(buf.size()),err))
            {
                std::cerr<<"[Export] Failed Nodes: "<<err<<"\n";
                return false;
            }
        }

        auto writer=create_file_writer(binPath.string());
        if(!writer)
        {
            std::cerr<<"[Export] Cannot open: "<<binPath<<"\n";
            return false;
        }

        MiniPackBuildResult result;
        if(!builder.build_pack(writer.get(),false,result,err))
        {
            std::cerr<<"[Export] Build pack failed: "<<err<<"\n";
            return false;
        }

        std::cout<<"[Export] Saved: "<<binPath<<"\n";
        return true;
    }

} // namespace pure
