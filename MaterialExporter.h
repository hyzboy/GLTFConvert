#pragma once

#include <filesystem>

namespace pure { struct Model; }

namespace exporters
{
    // 导出材质信息到指定目录下的 Materials.json
    // 返回是否成功
    bool ExportMaterials(const pure::Model &model,const std::filesystem::path &dir);
}
