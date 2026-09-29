#include "SceneExportGeometries.h"
#include "ExportFileNames.h"

namespace exporters
{
    void BuildGeometries(const CollectedIndices &ci,
                         const std::string &geometryBaseName,
                         int32_t totalGeometryCount,
                         SceneExportData &outData)
    {
        outData.geometries.reserve(ci.geometries.size());
        for (int32_t originalGeo : ci.geometries)
        {
            SceneGeometryExport ge;
            ge.originalIndex = originalGeo;
            ge.file          = MakeGeometryFileName(geometryBaseName, originalGeo, totalGeometryCount);
            outData.geometries.push_back(std::move(ge));
        }
    }
}
