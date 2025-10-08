#pragma once

#include <filesystem>
#include <vector>

#include "pure/Mesh.h"

namespace pure { struct Model; }

namespace exporters
{
    // 导出每个 mesh 的 json 描述文件:  文件名 = <meshName>.json  (若无名称则使用 mesh.<index>.json)
    // JSON 结构示例:
    // {
    //   "index": 0,
    //   "name": "MeshName",
    //   "primitives": [ { "primitiveIndex": 3, "geometry": "Base.5.geometry", "material": "Base.MatName.material" }, ... ]
    // }
    bool ExportMeshes(const pure::Model &model, const std::filesystem::path &dir);
}
