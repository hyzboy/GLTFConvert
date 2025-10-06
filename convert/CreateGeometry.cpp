#include <vector>

#include "convert/UniqueGeometryMapping.h"
#include "pure/Geometry.h"
#include "gltf/GLTFPrimitive.h"
#include "gltf/GLTFGeometry.h"

namespace pure
{
    void CreateUniqueGeometryEntries(std::vector<Geometry> &dstGeometry, const std::vector<GLTFPrimitive> &prims, const UniqueGeometryMapping &map)
    {
        dstGeometry.reserve(map.uniqueRepGeomPrimIdx.size());
        for (int32_t u = 0; u < static_cast<int32_t>(map.uniqueRepGeomPrimIdx.size()); ++u)
        {
            int32_t i = map.uniqueRepGeomPrimIdx[static_cast<size_t>(u)];
            const auto &g = prims[static_cast<size_t>(i)].geometry;

            Geometry pg;
            pg.primitiveType = g.primitiveType;

            if (!g.attributes.empty())
            {
                pg.attributes.reserve(g.attributes.size());
                // Copy slice derived -> base (data members from GeometryAttribute portion)
                pg.attributes.insert(pg.attributes.end(), g.attributes.begin(), g.attributes.end());
            }

            if (g.indices)
                pg.indicesData = *g.indices;
            if (g.indexCount && g.indexType != IndexType::ERR)
            {
                pg.indices = GeometryIndicesMeta{ *g.indexCount, g.indexType };
            }
            dstGeometry.push_back(std::move(pg));
        }
    }
} // namespace pure
