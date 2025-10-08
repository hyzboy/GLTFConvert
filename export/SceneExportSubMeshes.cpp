#include "SceneExportSubMeshes.h"
#include "SceneExportData.h"

#include "pure/Model.h"
#include "pure/SubMesh.h"

namespace exporters
{
    void BuildSubMeshes(const CollectedIndices &ci,
                        const std::string &baseName,
                        SceneExportData &outData)
    {
        outData.primitives.reserve(ci.primitives.size());
        for (int32_t original : ci.primitives)
        {
            ScenePrimitiveExport pe;
            pe.originalIndex = original;
            pe.geometryIndex = -1;
            pe.geometryFile  = baseName + "." + std::to_string(original) + ".geometry";
            outData.primitives.push_back(std::move(pe));
        }
    }
}
