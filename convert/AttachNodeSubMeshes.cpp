#include "convert/ConvertInternals.h"

namespace pure
{
    void AttachNodeSubMeshes(Model &dst,const GLTFModel &src)
    {
        for(int32_t ni=0; ni<static_cast<int32_t>(src.nodes.size()); ++ni)
        {
            const auto &node=src.nodes[static_cast<size_t>(ni)]; auto &pn=dst.mesh_nodes[static_cast<size_t>(ni)];
            if(node.mesh)
            {
                const auto &mesh=src.meshes[static_cast<size_t>(*node.mesh)]; pn.subMeshes.reserve(mesh.primitives.size());
                for(auto primIndex:mesh.primitives) pn.subMeshes.push_back(static_cast<int32_t>(primIndex));
            }
        }
    }
}
