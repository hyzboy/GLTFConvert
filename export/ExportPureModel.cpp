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
#include "MeshExporter.h" // added
#include "ExportFileNames.h"

namespace exporters
{
    void ExportGeometries(pure::Model *model,const std::filesystem::path &targetDir);

    static std::string stem_noext(const std::filesystem::path &p)
    {
        return p.stem().string();
    }

    // New extended version with flags
    bool ExportPureModel(pure::Model &sm,const std::filesystem::path &outDir,bool exportImagesFlag,bool imagesOnly)
    {
        std::filesystem::path baseDir=outDir.empty()
            ?std::filesystem::path(sm.model_source).parent_path()
            :outDir;

        std::error_code ec;
        std::filesystem::create_directories(baseDir,ec);

        const std::string baseName=stem_noext(sm.model_source);
        std::filesystem::path targetDir=baseDir/(baseName+".StaticMesh");
        std::filesystem::create_directories(targetDir,ec);

        const std::size_t sceneIndex=0; // first scene only

        // Collect indices for scene export when scenes are present. If there are
        // no scenes, we still want to export materials / geometries / meshes / images
        // so do not early-out here. Use an empty CollectedIndices when no scene is
        // available so downstream callers behave reasonably.
        CollectedIndices collected;
        if (!sm.scenes.empty())
        {
            collected = CollectSceneIndices(sm, sm.scenes[sceneIndex]);
        }

        // Gather used texture / image / sampler indices (still needed for image export filtering)
        std::vector<std::size_t> usedTextures; // unused now after removal of textures list export
        std::vector<std::size_t> usedImages;
        std::vector<std::size_t> usedSamplers; // unused now
        CollectUsedTextures(sm,collected,usedTextures,usedImages,usedSamplers);

        if(imagesOnly)
        {
            if(exportImagesFlag)
            {
                std::cout << "[Export] Images-only mode: exporting images...\n";
                ExportImages(sm,targetDir,&usedImages);
            }
            else
            {
                std::cout << "[Export] Images-only mode but exportImagesFlag==false (nothing to do).\n";
            }
            return true; // done
        }

        if(!ExportMaterials(sm,targetDir)) return false;
        ExportGeometries(&sm,targetDir);
        if(!ExportMeshes(sm,targetDir)) return false;

        if(exportImagesFlag)
        {
            ExportImages(sm,targetDir,&usedImages);
        }
        else
        {
            std::cout << "[Export] Image export disabled by command line flag.\n";
        }

        if(!sm.scenes.empty())
        {
            std::string sceneName=SanitizeName(sm.scenes[sceneIndex].name);
            if(sceneName.empty()) sceneName="scene"+std::to_string(sceneIndex);

            auto data=BuildSceneExportData(sm,sceneIndex,baseName);

            auto jsonPath=targetDir/MakeSceneJsonFileName(baseName,sceneName);
            if(!WriteSceneJson(data,jsonPath)) return false;

            auto packPath=targetDir/MakeScenePackFileName(baseName,sceneName);
            if(!WriteScenePack(data,packPath)) return false;
        }

        return true;
    }

    // Backward compatible wrapper (default: export images, not images-only)
    bool ExportPureModel(pure::Model &sm,const std::filesystem::path &outDir)
    {
        return ExportPureModel(sm,outDir,true,false);
    }
}
