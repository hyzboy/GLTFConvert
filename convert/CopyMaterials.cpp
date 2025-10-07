#include <vector>

#include "pure/Material.h"
#include "gltf/GLTFMaterial.h"

namespace pure
{
    void CopyMaterials(std::vector<Material> &dstMaterials, const std::vector<GLTFMaterial> &srcMaterials)
    {
        // 现在 pure::Material 等同于 GLTFMaterial，直接复制整个数组
        dstMaterials = srcMaterials;
    }
} // namespace pure
