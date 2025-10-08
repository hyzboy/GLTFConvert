#include "SceneExportPrimitives.h"
#include "SceneExportData.h"
#include "ExportFileNames.h"

namespace exporters
{
    void BuildPrimitivesExport(const CollectedIndices &ci,
                               const std::string &baseName,
                               SceneExportData &outData)
    {
        outData.primitives.reserve(ci.primitives.size());
        for (int32_t original : ci.primitives)
        {
            ScenePrimitiveExport pe;
            pe.originalIndex = original;
            pe.geometryIndex = -1;
            pe.geometryFile  = MakeGeometryFileName(baseName, original);
            outData.primitives.push_back(std::move(pe));
        }
    }
}
