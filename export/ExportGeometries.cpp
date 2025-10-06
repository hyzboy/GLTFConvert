#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "pure/Geometry.h"        // Geometry + SaveGeometry
#include "pure/ModelCore.h"       // BoundingBox + kInvalidBoundsIndex (could be moved to a smaller header if available)

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
    void ExportGeometries(const std::string &gltf_source,
                          const std::vector<pure::Geometry> &geometries,
                          const std::vector<BoundingBox> &boundsPool,
                          const std::filesystem::path &targetDir)
    {
        const std::string baseName = stem_noext(gltf_source);
        for(std::size_t u=0; u<geometries.size(); ++u)
        {
            const auto &g = geometries[u];
            std::filesystem::path binName = baseName + "." + std::to_string(u) + ".geometry";
            std::filesystem::path binPath = targetDir / binName;
            const BoundingBox &geo_bounds = (g.boundsIndex!=pure::kInvalidBoundsIndex)
                ? boundsPool[g.boundsIndex]
                : BoundingBox{};
            if(!pure::SaveGeometry(g, geo_bounds, binPath.string()))
                std::cerr << "[Export] Failed to write geometry binary: " << binPath << "\n";
        }
    }

} // namespace exporters
