#include "convert/ConvertInternals.h"

namespace pure
{
    void BuildSubMeshes(Model &dst,const GLTFModel &src,const UniqueGeometryMapping &map)
    {
        const auto &prims=src.primitives; dst.subMeshes.reserve(prims.size());
        for(int32_t i=0; i<static_cast<int32_t>(prims.size()); ++i)
        {
            const auto &p=prims[static_cast<size_t>(i)]; SubMesh sm; sm.geometry=map.geomIndexOfPrim[static_cast<size_t>(i)]; sm.material=p.material?std::optional<int32_t>(static_cast<int32_t>(*p.material)):std::optional<int32_t>{}; dst.subMeshes.push_back(std::move(sm));
        }
    }
}
