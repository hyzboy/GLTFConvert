#include <vector>

#include "pure/Node.h"
#include "gltf/GLTFNode.h"

namespace pure
{
    void CopyNodes(std::vector<Node> &dstNodes, const std::vector<GLTFNode> &srcNodes)
    {
        dstNodes.reserve(srcNodes.size());
        for (const auto &n : srcNodes)
        {
            Node pn;
            pn.name = n.name;
            pn.children.reserve(n.children.size());
            for (auto c : n.children)
                pn.children.push_back(static_cast<int32_t>(c));
            pn.transform = n.transform;
            if (n.mesh) pn.mesh = static_cast<int32_t>(*n.mesh);
            dstNodes.push_back(std::move(pn));
        }
    }
} // namespace pure
