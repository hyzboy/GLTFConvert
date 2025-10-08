#include <vector>
#include <optional>

#include "convert/UniqueGeometryMapping.h"
#include "pure/SubMesh.h" // defines Primitive
#include "gltf/GLTFPrimitive.h"

namespace pure
{
    void BuildPrimitives(std::vector<Primitive> &dst, const std::vector<GLTFPrimitive> &prims, const UniqueGeometryMapping &map)
    {
        dst.reserve(prims.size());
        for (int32_t i = 0; i < static_cast<int32_t>(prims.size()); ++i)
        {
            const auto &p = prims[static_cast<size_t>(i)];
            Primitive prim;
            prim.geometry = map.geomIndexOfPrim[static_cast<size_t>(i)];
            prim.material = p.material ? std::optional<int32_t>(static_cast<int32_t>(*p.material)) : std::optional<int32_t>{};
            dst.push_back(std::move(prim));
        }
    }
} // namespace pure
