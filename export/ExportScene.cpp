#include <filesystem>
#include <string>

#include "pure/Scene.h"
#include "pure/ModelCore.h"
#include "pure/MeshNode.h"
#include "SceneExporter.h"

namespace exporters
{
    namespace
    {
        static std::string SanitizeName(const std::string &in)
        {
            if(in.empty())
                return {};
            std::string out; out.reserve(in.size());
            for(char c:in)
            {
                if(std::isalnum(static_cast<unsigned char>(c))||c=='_'||c=='-')
                    out.push_back(c);
                else
                    out.push_back('_');
            }
            return out;
        }
    }

    bool ExportScene(const pure::Model &sm,
                     std::size_t si,
                     const std::filesystem::path &targetDir,
                     const std::string &baseName)
    {
        const auto &sc=sm.scenes[si];
        pure::SceneExporter sl=pure::SceneExporter::Build(sm,static_cast<int32_t>(si));

        std::string sceneFolderName;
        {
            std::string sane=SanitizeName(sc.name);
            if(sane.empty())
                sceneFolderName="Scene_"+std::to_string(si);
            else
                sceneFolderName=std::to_string(si)+"_"+sane;
        }

        std::filesystem::path sceneDir=targetDir/sceneFolderName;
        std::error_code ec; std::filesystem::create_directories(sceneDir,ec);

        if(!sl.WriteSceneBinary(sceneDir,baseName))
            return false;

        return true;
    }
} // namespace exporters
