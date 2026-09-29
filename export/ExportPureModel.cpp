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

    /// 选择要导出的场景：优先用 glTF 的默认场景（`scene` 字段，规范要求客户端优先使用它）。
    /// 此前写死 0 ⇒ 多场景资产导出的是**非默认场景**（实测 `MultipleScenes`：`scene=1`，
    /// 却导出了 scene 0 的节点/网格）。越界或未设置时回落到 0。
    static std::size_t SelectDefaultScene(const pure::Model &model)
    {
        if (model.scenes.empty())
            return 0;
        const int32_t ds = model.defaultScene;
        if (ds < 0 || ds >= static_cast<int32_t>(model.scenes.size()))
            return 0;
        return static_cast<std::size_t>(ds);
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

        const std::size_t sceneIndex = SelectDefaultScene(sm);

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
