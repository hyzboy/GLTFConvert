#pragma once

#include <filesystem>
#include <vector>

namespace pure { struct Material; }

namespace exporters
{
    // 导出材质信息到指定目录下的 Materials.json，仅基于材质数组
    bool ExportMaterials(const std::vector<pure::Material> &materials,const std::filesystem::path &dir);
}
