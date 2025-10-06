#include <filesystem>

#include "Exporter.h"
#include "MaterialExporter.h"
#include "pure/ModelCore.h"

namespace exporters
{
    // 外部实现声明
    void ExportGeometries(const pure::Model &sm,const std::filesystem::path &targetDir);
    bool ExportScene(const pure::Model &sm,std::size_t si,const std::filesystem::path &targetDir,const std::string &baseName);

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

        // 几何数据
        ExportGeometries(sm,targetDir);

        // 材质
        if(!ExportMaterials(sm.materials,targetDir))
            return false;

        // 场景
        for(std::size_t si=0; si<sm.scenes.size(); ++si)
        {
            if(!ExportScene(sm,si,targetDir,baseName))
                return false;
        }

        return true;
    }
} // namespace exporters
