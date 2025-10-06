#include <filesystem>
#include <string>
#include <cctype>

#include "SceneExporter.h"

namespace exporters
{
    namespace
    {
        static std::string SanitizeName(const std::string &in)
        {
            if(in.empty()) return {};
            std::string out; out.reserve(in.size());
            for(char c:in)
                out.push_back( (std::isalnum(static_cast<unsigned char>(c))||c=='_'||c=='-') ? c : '_' );
            return out;
        }
    }

    // 精简后的接口：不再接收整个 Model，只接收场景名称、索引、已构建的 SceneExporter
    bool ExportScene(const std::string &sceneName,
                     std::size_t sceneIndex,
                     const pure::SceneExporter &sceneExporter,
                     const std::filesystem::path &targetDir,
                     const std::string &baseName)
    {
        std::string sane = SanitizeName(sceneName);
        std::string sceneFolderName = sane.empty()
            ? ("Scene_" + std::to_string(sceneIndex))
            : (std::to_string(sceneIndex) + "_" + sane);

        std::filesystem::path sceneDir = targetDir / sceneFolderName;
        std::error_code ec; std::filesystem::create_directories(sceneDir, ec);

        if(!sceneExporter.WriteSceneBinary(sceneDir, baseName))
            return false;
        return true;
    }
} // namespace exporters
