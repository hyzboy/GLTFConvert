#include <fastgltf/core.hpp>
#include <vector>
#include "gltf/GLTFNode.h"
#include "gltf/ToNodeTransform.h"

namespace gltf
{
    bool ImportNodes(const fastgltf::Asset &asset,std::vector<GLTFNode> &nodes)
    {
        nodes.resize(asset.nodes.size());
        for(std::size_t i=0; i<asset.nodes.size(); ++i)
        {
            const auto &n=asset.nodes[i];
            auto &on=nodes[i];
            if(!n.name.empty()) on.name.assign(n.name.begin(),n.name.end());
            if(n.meshIndex) on.mesh=*n.meshIndex;
            on.children.assign(n.children.begin(),n.children.end());

            // fail-fast：局部变换无法用 TRS 表示（剪切/两轴以上退化）时中止导入，
            // 不产出"半成品模型"（错误信息由 ToNodeTransform 打印，含节点名与残差）
            if(!ToNodeTransform(n.transform,i,on.name,on.transform))
                return false;
        }

        return true;
    }
} // namespace gltf
