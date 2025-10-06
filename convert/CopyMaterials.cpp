#include <vector>

#include "pure/Material.h"
#include "gltf/GLTFMaterial.h"

namespace pure
{
    void CopyMaterials(std::vector<Material> &dstMaterials, const std::vector<GLTFMaterial> &srcMaterials)
    {
        dstMaterials.reserve(srcMaterials.size());
        for (const auto &m : srcMaterials)
        {
            Material pm;
            pm.name = m.name;
            dstMaterials.push_back(std::move(pm));
        }
    }
} // namespace pure
