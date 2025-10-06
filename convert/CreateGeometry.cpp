#include <vector>

#include "convert/UniqueGeometryMapping.h"
#include "pure/Geometry.h"
#include "gltf/Primitive.h"
#include "gltf/Geometry.h"

namespace pure
{
    namespace
    {
        static GeometryAttribute MakeGeometryAttribute(const GLTFGeometry::GLTFGeometryAttribute &srcAttr, uint8_t id)
        {
            GeometryAttribute ga;
            ga.id = id;
            ga.name = srcAttr.name;
            ga.count = srcAttr.count;
            ga.componentType = srcAttr.componentType;
            ga.type = srcAttr.type;
            ga.format = srcAttr.format;
            ga.data = srcAttr.data; // copy raw data
            return ga;
        }
    } // anonymous namespace

    void CreateUniqueGeometryEntries(std::vector<Geometry> &dstGeometry, const std::vector<GLTFPrimitive> &prims, const UniqueGeometryMapping &map)
    {
        dstGeometry.reserve(map.uniqueRepGeomPrimIdx.size());
        for (int32_t u = 0; u < static_cast<int32_t>(map.uniqueRepGeomPrimIdx.size()); ++u)
        {
            int32_t i = map.uniqueRepGeomPrimIdx[static_cast<size_t>(u)];
            const auto &g = prims[static_cast<size_t>(i)].geometry;

            Geometry pg;
            pg.mode = g.mode;
            pg.attributes.reserve(g.attributes.size());
            for (size_t ai = 0; ai < g.attributes.size(); ++ai)
            {
                pg.attributes.push_back(MakeGeometryAttribute(g.attributes[ai], static_cast<uint8_t>(ai)));
            }
            if (g.indices)
                pg.indicesData = *g.indices;
            if (g.indexCount && g.indexComponentType)
            {
                pg.indices = GeometryIndicesMeta{ *g.indexCount, *g.indexComponentType };
                pg.indices->indexType = g.indexType;
            }
            dstGeometry.push_back(std::move(pg));
        }
    }
} // namespace pure
