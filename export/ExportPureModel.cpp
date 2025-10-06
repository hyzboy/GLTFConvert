#include <filesystem>
#include <string>
#include <vector>

#include "Exporter.h"
#include "MaterialExporter.h"
#include "pure/ModelCore.h"
#include "SceneExporter.h"

namespace exporters
{
    // 新签名：几何导出
    void ExportGeometries(const std::string &gltf_source,
                          const std::vector<pure::Geometry> &geometries,
                          const std::vector<BoundingBox> &boundsPool,
                          const std::filesystem::path &targetDir);

    // 场景导出（精简接口）
    bool ExportScene(const std::string &sceneName,
                     std::size_t sceneIndex,
                     const pure::SceneExporter &sceneExporter,
                     const std::filesystem::path &targetDir,
                     const std::string &baseName);

    static std::string stem_noext(const std::filesystem::path &p)
    {
        return p.stem().string();
    }

    bool ExportPureModel(const GLTFModel &model,const std::filesystem::path &outDir)
    {
        pure::Model sm=pure::ConvertFromGLTF(model);

        std::filesystem::path baseDir=outDir.empty()
            ?std::filesystem::path(sm.gltf_source).parent_path()
            :outDir;
        std::error_code ec;
        std::filesystem::create_directories(baseDir,ec);

        const std::string baseName=stem_noext(sm.gltf_source);
        std::filesystem::path targetDir=baseDir/(baseName+".StaticMesh");
        std::filesystem::create_directories(targetDir,ec);

        // 几何数据（使用精简后的接口）
        ExportGeometries(sm.gltf_source, sm.geometry, sm.bounds, targetDir);

        // 材质
        if(!ExportMaterials(sm.materials,targetDir))
            return false;

        // 场景
        for(std::size_t si=0; si<sm.scenes.size(); ++si)
        {
            const auto &scene = sm.scenes[si];
            pure::SceneExporter sceneExporter = pure::SceneExporter::Build(sm, static_cast<int32_t>(si));
            if(!ExportScene(scene.name, si, sceneExporter, targetDir, baseName))
                return false;
        }

        return true;
    }
} // namespace exporters
