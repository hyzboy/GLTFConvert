#include <vector>

#include "pure/Mesh.h"
#include "gltf/GLTFMesh.h"

namespace pure
{
    void BuildMeshes(std::vector<Mesh> &dstMeshes, const std::vector<GLTFMesh> &srcMeshes)
    {
        dstMeshes.reserve(srcMeshes.size());
        for (const auto &m : srcMeshes)
        {
            Mesh pm;
            pm.name = m.name;
            pm.primitives.reserve(m.primitives.size());
            for (auto prim : m.primitives)
                pm.primitives.push_back(static_cast<int32_t>(prim));
            dstMeshes.push_back(std::move(pm));
        }
    }
} // namespace pure
