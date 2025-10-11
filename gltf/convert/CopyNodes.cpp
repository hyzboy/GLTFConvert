#include <vector>

#include "gltf/GLTFNode.h"
#include "pure/Node.h"

namespace gltf
{
    void CopyNodes(std::vector<pure::Node> &dstNodes, const std::vector<GLTFNode> &srcNodes)
    {
        dstNodes.reserve(srcNodes.size());
        for (const auto &n : srcNodes)
        {
            pure::Node pn;
            pn.name = n.name;
            pn.children.reserve(n.children.size());
            for (auto c : n.children)
                pn.children.push_back(static_cast<int32_t>(c));
            pn.transform = n.transform;
            if (n.mesh) pn.mesh = static_cast<int32_t>(*n.mesh);
            dstNodes.push_back(std::move(pn));
        }
    }
} // namespace gltf
