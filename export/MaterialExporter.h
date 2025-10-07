#pragma once

#include <filesystem>
#include <vector>

#include "pure/Material.h" // now Material is alias to GLTFMaterial

namespace exporters
{
    // 导出：每个材质独立 .material 文件（baseName.materialName.material）
    bool ExportMaterials(const std::vector<pure::Material> &materials,const std::filesystem::path &dir);
}
