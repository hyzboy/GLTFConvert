#include "convert/ConvertInternals.h"

namespace pure
{
    void CreateUniqueGeometryEntries(Model &dst,const GLTFModel &src,const UniqueGeometryMapping &map)
    {
        const auto &prims=src.primitives; dst.geometry.reserve(map.uniqueRepGeomPrimIdx.size());
        for(int32_t u=0; u<static_cast<int32_t>(map.uniqueRepGeomPrimIdx.size()); ++u)
        {
            int32_t i=map.uniqueRepGeomPrimIdx[static_cast<size_t>(u)]; const auto &g=prims[static_cast<size_t>(i)].geometry; Geometry pg; pg.mode=g.mode; pg.attributes.reserve(g.attributes.size());
            for(size_t ai=0; ai<g.attributes.size(); ++ai){ const auto &a=g.attributes[ai]; GeometryAttribute ga; ga.id=static_cast<uint8_t>(ai); ga.name=a.name; ga.count=a.count; ga.componentType=a.componentType; ga.type=a.type; ga.format=a.format; ga.data=a.data; pg.attributes.push_back(std::move(ga)); }
            if(g.indices) pg.indicesData=*g.indices; if(g.indexCount&&g.indexComponentType){ pg.indices=GeometryIndicesMeta{ *g.indexCount,*g.indexComponentType }; pg.indices->indexType=g.indexType; }
            dst.geometry.push_back(std::move(pg));
        }
    }
}
