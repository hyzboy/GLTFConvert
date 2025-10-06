#include <vector>
#include <optional>

#include "convert/UniqueGeometryMapping.h"
#include "pure/SubMesh.h"
#include "gltf/GLTFPrimitive.h"

namespace pure
{
    void BuildSubMeshes(std::vector<SubMesh> &dstSubMeshes, const std::vector<GLTFPrimitive> &prims, const UniqueGeometryMapping &map)
    {
        dstSubMeshes.reserve(prims.size());
        for (int32_t i = 0; i < static_cast<int32_t>(prims.size()); ++i)
        {
            const auto &p = prims[static_cast<size_t>(i)];
            SubMesh sm;
            sm.geometry = map.geomIndexOfPrim[static_cast<size_t>(i)];
            sm.material = p.material ? std::optional<int32_t>(static_cast<int32_t>(*p.material)) : std::optional<int32_t>{};
            dstSubMeshes.push_back(std::move(sm));
        }
    }
} // namespace pure
