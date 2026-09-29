#include <filesystem>
#include <algorithm>
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

        // 场景集合：多场景时**全部**导出（A4），所以图像过滤要取**所有场景**的并集 ——
        // 否则"只被非默认场景引用的贴图"会被漏掉，那个场景的产物就会缺贴图。
        CollectedIndices collected;
        if (!sm.scenes.empty())
        {
            auto merge=[&](std::vector<int32_t> &dst, const std::vector<int32_t> &src)
            {
                dst.insert(dst.end(), src.begin(), src.end());
                std::sort(dst.begin(), dst.end());
                dst.erase(std::unique(dst.begin(), dst.end()), dst.end());
            };
            for (const auto &scene : sm.scenes)
            {
                const CollectedIndices one = CollectSceneIndices(sm, scene);
                merge(collected.nodes, one.nodes);
                merge(collected.primitives, one.primitives);
                merge(collected.materials, one.materials);
                merge(collected.geometries, one.geometries);
            }
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
            // 多场景：逐个导出，命名 `<base>.scene<N>.json/.scene`（用户拍板口径 `sceneN`）；
            // 单场景：保持既有命名（`SanitizeName(name)`，无名时 SanitizeName 内部返回 "unnamed"）
            //         ⇒ 现有产物文件名零变动。
            // ⚠ 必须自己判"源名是否为空"来决定用 sceneN：`SanitizeName("")` 会返回 "unnamed"
            //   （SanitizeName.cpp:21），多个无名场景会因此撞成同一个文件名（实测 `MultipleScenes`）。
            const bool multiScene = sm.scenes.size() > 1;
            for(std::size_t si=0; si<sm.scenes.size(); ++si)
            {
                const std::string sceneName = multiScene
                                            ? ("scene" + std::to_string(si))
                                            : SanitizeName(sm.scenes[si].name);

                auto data=BuildSceneExportData(sm,si,baseName);

                auto jsonPath=targetDir/MakeSceneJsonFileName(baseName,sceneName);
                if(!WriteSceneJson(data,jsonPath)) return false;

                auto packPath=targetDir/MakeScenePackFileName(baseName,sceneName);
                if(!WriteScenePack(data,packPath)) return false;
            }
        }

        return true;
    }

    // Backward compatible wrapper (default: export images, not images-only)
    bool ExportPureModel(pure::Model &sm,const std::filesystem::path &outDir)
    {
        return ExportPureModel(sm,outDir,true,false);
    }
}
