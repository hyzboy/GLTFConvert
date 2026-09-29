#include "SceneExportPrimitives.h"
#include "SceneExportData.h"

namespace exporters
{
    // 几何文件名**不在这里算**：它取自几何表（`data.geometries[].file`，见 SceneExportBuild 的链接步），
    // 保证与写出侧（`ExportGeometries`）用的是同一条命名规则。此前这里按默认 total=-1 自己算过一份
    // （永远带索引），导致单几何资产的包内名字（X.0.geometry）与磁盘（X.geometry）不符 ⇒ 引擎加载失败。
    void BuildPrimitivesExport(const CollectedIndices &ci,
                               SceneExportData &outData)
    {
        outData.primitives.reserve(ci.primitives.size());
        for (int32_t original : ci.primitives)
        {
            ScenePrimitiveExport pe;
            pe.originalIndex = original;
            pe.geometryIndex = -1;
            outData.primitives.push_back(std::move(pe));
        }
    }
}
