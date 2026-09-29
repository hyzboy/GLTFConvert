#pragma once

#include <string>
#include "SceneExportData.h"
#include "SceneExportCollect.h"

namespace exporters
{
    // Build exported primitive entries
    // 几何文件名由几何表提供（见 SceneExportBuild 的链接步），此处不再生成。
    void BuildPrimitivesExport(const CollectedIndices &ci,
                               SceneExportData &outData);
}
