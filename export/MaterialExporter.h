#pragma once

#include <filesystem>
#include <vector>

#include "pure/Material.h" // alias to GLTFMaterial
namespace pure { struct Model; }

namespace exporters
{
    // 导出：每个材质独立 .material 文件（包含该材质用到的本地 images/textures/samplers 重新编号）
    bool ExportMaterials(const pure::Model &model, const std::filesystem::path &dir);
}
