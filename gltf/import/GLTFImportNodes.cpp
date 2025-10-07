#include <fastgltf/core.hpp>
#include <vector>
#include "gltf/GLTFNode.h"
#include "gltf/ToNodeTransform.h"

namespace gltf
{
    void ImportNodes(const fastgltf::Asset &asset,std::vector<GLTFNode> &nodes)
    {
        nodes.resize(asset.nodes.size());
        for(std::size_t i=0; i<asset.nodes.size(); ++i)
        {
            const auto &n=asset.nodes[i];
            auto &on=nodes[i];
            if(!n.name.empty()) on.name.assign(n.name.begin(),n.name.end());
            if(n.meshIndex) on.mesh=*n.meshIndex;
            on.children.assign(n.children.begin(),n.children.end());
            on.transform = ToNodeTransform(n.transform);
        }
    }
} // namespace gltf
