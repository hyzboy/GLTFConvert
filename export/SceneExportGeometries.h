#pragma once

#include <string>

#include "SceneExportData.h"
#include "SceneExportCollect.h"

namespace exporters
{
    /// totalGeometryCount 必须与**写出侧**（`ExportGeometries`，用 `model.geometry.size()`）一致：
    /// `MakeGeometryFileName` 在 total==1 时不带索引，若这里传"本场景收集到的几何数"，
    /// 多 scene 资产会算出与磁盘不同的名字（实测 `MultipleScenes`：写出 `MultipleScenes.0/1.geometry`，
    /// 记录 `MultipleScenes.geometry` ⇒ 场景包打开几何文件失败）。
    void BuildGeometries(const CollectedIndices &ci,
                         const std::string &geometryBaseName,
                         int32_t totalGeometryCount,
                         SceneExportData &outData);
}
