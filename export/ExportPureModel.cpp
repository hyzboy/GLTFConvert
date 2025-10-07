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
            auto data = BuildSceneExportData(sm, si, baseName);

            auto jsonPath = targetDir / ("Scene" + std::to_string(si) + ".json");
            if (!WriteSceneJson(data, jsonPath))
                return false;

            auto packPath = targetDir / ("Scene" + std::to_string(si) + ".scenepack");
            if (!WriteScenePack(data, packPath))
                return false;
        }
        return true;
    }
}
