#include "pure/ModelCore.h"
#include "convert/ConvertInternals.h"

namespace pure
{
    Model ConvertFromGLTF(const GLTFModel &src)
    {
        Model dst; dst.gltf_source=src.source;
        CopyMaterials(dst,src);
        CopyScenes(dst,src);
        CopyMeshNodesAndTransforms(dst,src);
        auto uniqueMap=BuildUniqueGeometryMapping(src);
        CreateUniqueGeometryEntries(dst,src,uniqueMap);
        BuildSubMeshes(dst,src,uniqueMap);
        AttachNodeSubMeshes(dst,src);
        return dst;
    }
} // namespace pure
