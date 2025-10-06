#include <vector>

#include "pure/MeshNode.h"
#include "gltf/GLTFNode.h"

namespace pure
{
    void CopyMeshNodesAndTransforms(std::vector<MeshNode> &dstNodes, const std::vector<GLTFNode> &srcNodes)
    {
        dstNodes.reserve(srcNodes.size());
        for (const auto &n : srcNodes)
        {
            MeshNode pn;
            pn.name = n.name;
            pn.children.reserve(n.children.size());
            for (auto c : n.children)
                pn.children.push_back(static_cast<int32_t>(c));
            pn.node_transform = n.transform;
            dstNodes.push_back(std::move(pn));
        }
    }
} // namespace pure
