#include "SceneExportMaterials.h"

#include "pure/Model.h"
#include <filesystem>
#include "ExportFileNames.h"

namespace exporters
{
    void BuildMaterials(const pure::Model &model,
                        const CollectedIndices &ci,
                        std::unordered_map<std::string, int32_t> &nameToIndex,
                        SceneExportData &outData)
    {
        (void)nameToIndex; // no longer needed for materials
        outData.materials.reserve(ci.materials.size());

        std::string baseName = GetBaseName(model);

        for (int32_t originalMat : ci.materials)
        {
            const auto &mat = model.materials[originalMat];
            SceneMaterialExport me;
            me.originalIndex = originalMat;
            me.file = MakeMaterialFileName(baseName, mat.name, originalMat);
            outData.materials.push_back(std::move(me));
        }
    }
}
