#include "SceneExportGeometries.h"

namespace exporters
{
    void BuildGeometries(const CollectedIndices &ci,
                         const std::string &geometryBaseName,
                         SceneExportData &outData)
    {
        outData.geometries.reserve(ci.geometries.size());
        for (int32_t originalGeo : ci.geometries)
        {
            SceneGeometryExport ge;
            ge.originalIndex = originalGeo;
            ge.file          = geometryBaseName + "." + std::to_string(originalGeo) + ".geometry";
            outData.geometries.push_back(std::move(ge));
        }
    }
}
