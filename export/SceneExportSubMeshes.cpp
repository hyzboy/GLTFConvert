#include "SceneExportSubMeshes.h"

#include "pure/Model.h"
#include "pure/SubMesh.h"

namespace exporters
{
    void BuildSubMeshes(const pure::Model &model,
                        const CollectedIndices &ci,
                        const RemapTables &remap,
                        const std::string &geometryBaseName,
                        SceneExportData &outData)
    {
        outData.subMeshes.reserve(ci.subMeshes.size());
        for (int32_t originalSM : ci.subMeshes)
        {
            const auto &sm = model.subMeshes[originalSM];
            SceneSubMeshExport se;
            se.originalIndex = originalSM;

            if (sm.geometry >= 0)
            {
                auto gIt = remap.geometryRemap.find(sm.geometry);
                se.geometryIndex = (gIt != remap.geometryRemap.end()) ? gIt->second : -1;
                se.geometryFile  = geometryBaseName + "." + std::to_string(sm.geometry) + ".geometry";
            }
            else
            {
                se.geometryIndex = -1;
            }

            if (sm.material.has_value())
            {
                auto mIt = remap.materialRemap.find(sm.material.value());
                if (mIt != remap.materialRemap.end())
                    se.materialIndex = mIt->second;
            }

            outData.subMeshes.push_back(std::move(se));
        }
    }
}
