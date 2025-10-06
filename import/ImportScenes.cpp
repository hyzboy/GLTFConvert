#include "ImportScenes.h"

namespace importers {

void ImportScenes(const fastgltf::Asset& asset, std::vector<GLTFScene>& scenes)
{
    scenes.resize(asset.scenes.size());
    for (std::size_t i = 0; i < asset.scenes.size(); ++i) {
        const auto& sc = asset.scenes[i];
        auto& os = scenes[i];
        if (!sc.name.empty()) os.name.assign(sc.name.begin(), sc.name.end());
        os.nodes.assign(sc.nodeIndices.begin(), sc.nodeIndices.end());
    }
}

} // namespace importers
