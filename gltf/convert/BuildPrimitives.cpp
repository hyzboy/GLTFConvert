#include <vector>

#include "gltf/GLTFPrimitive.h"
#include "pure/Geometry.h"
#include "pure/Primitive.h"
#include "gltf/convert/UniqueGeometryMapping.h"

namespace gltf
{
    void BuildPrimitives(std::vector<pure::Primitive> &dst, const std::vector<GLTFPrimitive> &prims, const UniqueGeometryMapping &map)
    {
        dst.reserve(prims.size());
        for (int32_t i = 0; i < static_cast<int32_t>(prims.size()); ++i)
        {
            const auto &p = prims[static_cast<size_t>(i)];
            pure::Primitive prim;
            prim.geometry = map.geomIndexOfPrim[static_cast<size_t>(i)];
            prim.material = p.material ? std::optional<int32_t>(static_cast<int32_t>(*p.material)) : std::optional<int32_t>{};
            dst.push_back(std::move(prim));
        }
    }
} // namespace gltf
