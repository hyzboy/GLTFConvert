#include "convert/ConvertInternals.h"

namespace pure
{
    void CopyMeshNodesAndTransforms(Model &dst,const GLTFModel &src)
    {
        dst.mesh_nodes.reserve(src.nodes.size());
        for(const auto &n:src.nodes)
        {
            MeshNode pn; pn.name=n.name; pn.children.reserve(n.children.size());
            for(auto c:n.children) pn.children.push_back(static_cast<int32_t>(c));
            pn.node_transform=n.transform; dst.mesh_nodes.push_back(std::move(pn));
        }
    }
}
