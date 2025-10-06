#include "ImportMaterials.h"

namespace importers {

void ImportMaterials(const fastgltf::Asset& asset, std::vector<GLTFMaterial>& materials)
{
    materials.clear();
    materials.reserve(asset.materials.size());
    for (const auto& m : asset.materials)
    {
        GLTFMaterial om{};
        if (!m.name.empty())
            om.name.assign(m.name.begin(), m.name.end());
        materials.emplace_back(std::move(om));
    }
}

} // namespace importers
