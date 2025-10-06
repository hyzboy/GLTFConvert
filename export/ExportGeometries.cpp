#include <filesystem>
#include <iostream>

#include "pure/ModelCore.h"
#include "pure/Geometry.h"
#include "pure/ModelCore.h"
#include "pure/Scene.h"
#include "pure/MeshNode.h"

namespace exporters
{
    namespace
    {
        static std::string stem_noext(const std::filesystem::path &p)
        {
            return p.stem().string();
        }
    }

    void ExportGeometries(const pure::Model &sm,const std::filesystem::path &targetDir)
    {
        for(std::size_t u=0; u<sm.geometry.size(); ++u)
        {
            const auto &g=sm.geometry[u];
            std::filesystem::path binName=stem_noext(sm.gltf_source)+"."+std::to_string(u)+".geometry";
            std::filesystem::path binPath=targetDir/binName;
            const BoundingBox &geo_bounds=(g.boundsIndex!=pure::kInvalidBoundsIndex)
                ?sm.bounds[g.boundsIndex]
                :BoundingBox{};
            if(!pure::SaveGeometry(g,geo_bounds,binPath.string()))
                std::cerr<<"[Export] Failed to write geometry binary: "<<binPath<<"\n";
        }
    }

} // namespace exporters
