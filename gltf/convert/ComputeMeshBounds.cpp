#include "ComputeMeshBounds.h"
#include "pure/Model.h"
#include "pure/Geometry.h"
#include <glm/glm.hpp>

namespace convert
{
    void ComputeMeshBounds(pure::Model &model)
    {
        // For each mesh, aggregate positions from all referenced primitives' geometry and compute bounds
        for (std::size_t mi = 0; mi < model.meshes.size(); ++mi)
        {
            auto &mesh = model.meshes[mi];
            std::vector<glm::vec3> pts;
            for (int32_t primIndex : mesh.primitives)
            {
                if (primIndex < 0 || primIndex >= static_cast<int32_t>(model.primitives.size()))
                    continue;
                const auto &prim = model.primitives[primIndex];
                if (prim.geometry < 0 || prim.geometry >= static_cast<int32_t>(model.geometry.size()))
                    continue;
                const auto &geom = model.geometry[prim.geometry];
                if (geom.positions.has_value())
                {
                    const auto &pos = geom.positions.value();
                    pts.insert(pts.end(), pos.begin(), pos.end());
                }
            }
            if(!pts.empty())
            {
                BoundingVolumes bv;
                bv.fromPoints(pts);
                mesh.bounding_volume = std::move(bv);
            }
        }
    }
}
