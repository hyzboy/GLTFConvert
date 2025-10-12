#include "fbx/import/FBX2Pure.h"

#include <glm/glm.hpp>
#include <algorithm>

namespace fbx
{
    using namespace pure;

    // Only geometry conversion for now
    pure::Model ConvertFromFBX(const FBXModel &src)
    {
        pure::Model dst;
        dst.gltf_source = src.gltf_source;

        // Copy images/textures/samplers as empty for now
        // Convert geometry: each FBX geometry entry becomes pure::Geometry
        dst.geometry.reserve(src.geometry.size());
        for (const auto &g : src.geometry)
        {
            pure::Geometry pg = g; // shallow copy of structure (positions/attributes/indices already in place)
            // Ensure positions are set if available in attributes
            if (!pg.positions.has_value()) {
                // try to find POSITION attribute and extract
            }
            dst.geometry.push_back(std::move(pg));
        }

        // Create primitives referencing geometries
        dst.primitives.reserve(dst.geometry.size());
        for (size_t i = 0; i < dst.geometry.size(); ++i)
        {
            pure::Primitive prim;
            prim.geometry = static_cast<int32_t>(i);
            dst.primitives.push_back(prim);
        }

        // Meshes: each geometry -> one mesh containing a single primitive
        dst.meshes.reserve(dst.primitives.size());
        for (size_t i = 0; i < dst.primitives.size(); ++i)
        {
            pure::Mesh m;
            m.name = "mesh_" + std::to_string(i);
            m.primitives.push_back(static_cast<int32_t>(i));
            // compute mesh bounds from geometry positions if available
            if (dst.geometry[i].positions.has_value() && !dst.geometry[i].positions->empty()) {
                auto &pos = *dst.geometry[i].positions;
                glm::vec3 mn = pos[0];
                glm::vec3 mx = pos[0];
                for (const auto &p : pos) { mn = glm::min(mn, p); mx = glm::max(mx, p); }
                m.bounding_volume.aabb.min = mn;
                m.bounding_volume.aabb.max = mx;
            }
            dst.meshes.push_back(std::move(m));
        }

        return dst;
    }
}
