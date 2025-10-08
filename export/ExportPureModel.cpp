#include <filesystem>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <unordered_set>

#include "MaterialExporter.h"
#include "pure/Model.h"
#include "SceneExportData.h"
#include "SceneExportCollect.h"
#include "SceneExportCollectTextures.h"
#include "ExportImages.h"
#include "ExportTextures.h"

namespace exporters
{
    void ExportGeometries(pure::Model *model, const std::filesystem::path &targetDir);

    static std::string stem_noext(const std::filesystem::path &p)
    {
        return p.stem().string();
    }

    static std::string sanitize_name(const std::string &n)
    {
        if (n.empty()) return {};
        std::string out = n;
        for (char &c : out)
        {
            switch (c)
            {
                case '/': case '\\': case ':': case '*': case '?':
                case '"': case '<':  case '>': case '|':
                    c = '_';
                    break;
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

        if (sm.scenes.empty())
            return false; // nothing to export

        // Currently export ONLY the first scene (context-filtered)
        const std::size_t sceneIndex = 0;
        auto collected = CollectSceneIndices(sm, sm.scenes[sceneIndex]);

        // Gather used texture / image / sampler indices
        std::vector<std::size_t> usedTextures;
        std::vector<std::size_t> usedImages;
        std::vector<std::size_t> usedSamplers;
        CollectUsedTextures(sm, collected, usedTextures, usedImages, usedSamplers);

        if (!ExportMaterials(sm.materials, targetDir))
            return false;
        ExportGeometries(&sm, targetDir);

        std::string sceneName = sanitize_name(sm.scenes[sceneIndex].name);
        if (sceneName.empty())
            sceneName = "scene" + std::to_string(sceneIndex);

        // Filtered resource exports
        ExportImages(sm, targetDir, &usedImages);
        ExportTexturesJson(sm.gltf_source, sm.images, sm.textures, sm.samplers,
                           &usedImages, &usedTextures, &usedSamplers, targetDir, sceneName);

        // Scene export data (json + pack)
        auto data = BuildSceneExportData(sm, sceneIndex, baseName);

        auto jsonPath = targetDir / (baseName + "." + sceneName + ".json");
        if (!WriteSceneJson(data, jsonPath))
            return false;

        auto packPath = targetDir / (baseName + "." + sceneName + ".scene");
        if (!WriteScenePack(data, packPath))
            return false;

        return true;
    }
}
