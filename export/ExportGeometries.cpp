#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "pure/Geometry.h"        // Geometry + SaveGeometry
#include "pure/Model.h"       // BoundingBox + kInvalidBoundsIndex (could be moved to a smaller header if available)

namespace exporters
{
    namespace
    {
        static std::string stem_noext(const std::filesystem::path &p)
        {
            return p.stem().string();
        }
    }

    // 仅导出几何数据；不再传递整个 Model，改为传递需要的最小集合
    void ExportGeometries(pure::Model *model,const std::filesystem::path &targetDir)
    {
        const std::string baseName = stem_noext(model->gltf_source);

        for(std::size_t u=0; u<model->geometry.size(); ++u)
        {
            pure::Geometry &g = model->geometry[u];

            std::filesystem::path binName = baseName + "." + std::to_string(u) + ".geometry";
            std::filesystem::path binPath = targetDir / binName;

            if(!pure::SaveGeometry(g, binPath.string()))
                std::cerr << "[Export] Failed to write geometry binary: " << binPath << "\n";
        }
    }
} // namespace exporters
