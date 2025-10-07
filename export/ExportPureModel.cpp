#include <filesystem>
#include <string>
#include <vector>

#include "MaterialExporter.h"
#include "pure/Model.h"
#include "SceneExportData.h" // scene export data + serialization declarations

namespace exporters
{
    void ExportGeometries(pure::Model *model, const std::filesystem::path &targetDir);

    static std::string stem_noext(const std::filesystem::path &p)
    {
        return p.stem().string();
    }

    static std::string sanitize_name(const std::string &n)
    {
        if (n.empty()) return std::string();
        std::string out=n;
        for(char &c:out)
        {
            switch(c)
            {
            case '/':case '\\':case ':':case '*':case '?':case '"':case '<':case '>':case '|':
                c='_'; break;
            default: break;
            }
        }
        return out;
    }

    bool ExportPureModel(pure::Model &sm, const std::filesystem::path &outDir)
    {
        std::filesystem::path baseDir = outDir.empty()
            ? std::filesystem::path(sm.gltf_source).parent_path()
            : outDir;

        std::error_code ec;
        std::filesystem::create_directories(baseDir, ec);

        const std::string baseName = stem_noext(sm.gltf_source);
        std::filesystem::path targetDir = baseDir / (baseName + ".StaticMesh");
        std::filesystem::create_directories(targetDir, ec);

        if (!ExportMaterials(sm.materials, targetDir))
            return false;

        ExportGeometries(&sm, targetDir);

        for (std::size_t si = 0; si < sm.scenes.size(); ++si)
        {
            const auto &scene = sm.scenes[si];
            auto data = BuildSceneExportData(sm, si, baseName);

            std::string sceneName = sanitize_name(scene.name);
            if (sceneName.empty()) // fallback to index if no name
                sceneName = "scene" + std::to_string(si);

            auto jsonPath = targetDir / (baseName + "." + sceneName + ".json");
            if (!WriteSceneJson(data, jsonPath))
                return false;

            auto packPath = targetDir / (baseName + "." + sceneName + ".scene");
            if (!WriteScenePack(data, packPath))
                return false;
        }
        return true;
    }
}
