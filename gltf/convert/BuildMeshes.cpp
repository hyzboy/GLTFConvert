#include <vector>

#include "gltf/GLTFMesh.h"
#include "pure/Mesh.h"

namespace gltf
{
    void BuildMeshes(std::vector<pure::Mesh> &dstMeshes, const std::vector<GLTFMesh> &srcMeshes)
    {
        dstMeshes.reserve(srcMeshes.size());
        for (const auto &m : srcMeshes)
        {
            pure::Mesh pm;
            pm.name = m.name;
            pm.primitives.reserve(m.primitives.size());
            for (auto prim : m.primitives)
                pm.primitives.push_back(static_cast<int32_t>(prim));
            dstMeshes.push_back(std::move(pm));
        }
    }
} // namespace gltf
