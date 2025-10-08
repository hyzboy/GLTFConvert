#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "pure/Geometry.h"        // Geometry + SaveGeometry
#include "pure/Model.h"       // BoundingBox + kInvalidBoundsIndex (could be moved to a smaller header if available)
#include "ExportFileNames.h"

namespace exporters
{
    // 仅导出几何数据；不再传递整个 Model，改为传递需要的最小集合
    void ExportGeometries(pure::Model *model,const std::filesystem::path &targetDir)
    {
        const std::string baseName = model->GetBaseName();

        for(std::size_t u=0; u<model->geometry.size(); ++u)
        {
            pure::Geometry &g = model->geometry[u];

            std::filesystem::path binName = MakeGeometryFileName(baseName, static_cast<int32_t>(u));
            std::filesystem::path binPath = targetDir / binName;

            if(!pure::SaveGeometry(g, binPath.string()))
                std::cerr << "[Export] Failed to write geometry binary: " << binPath << "\n";
        }
    }
} // namespace exporters
