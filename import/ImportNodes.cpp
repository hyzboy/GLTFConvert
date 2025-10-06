#include "ImportNodes.h"

namespace importers {

void ImportNodes(const fastgltf::Asset& asset, GLTFModel& outModel)
{
    outModel.nodes.resize(asset.nodes.size());
    for (std::size_t i = 0; i < asset.nodes.size(); ++i) {
        const auto& n = asset.nodes[i];
        auto& on = outModel.nodes[i];
        if (!n.name.empty()) on.name.assign(n.name.begin(), n.name.end());
        if (n.meshIndex) on.mesh = *n.meshIndex;
        on.children.assign(n.children.begin(), n.children.end());
        on.transform = n.transform;
    }
}

} // namespace importers
