#include "SceneExportMaterials.h"

#include "pure/Model.h"

namespace exporters
{
    void BuildMaterials(const pure::Model &model,
                        const CollectedIndices &ci,
                        std::unordered_map<std::string, int32_t> &nameToIndex,
                        SceneExportData &outData)
    {
        outData.materials.reserve(ci.materials.size());
        for (int32_t originalMat : ci.materials)
        {
            const auto &mat = model.materials[originalMat];
            SceneMaterialExport me;
            me.originalIndex = originalMat;
            me.nameIndex     = GetOrAddName(nameToIndex, outData.nameTable, mat.name);
            outData.materials.push_back(std::move(me));
        }
    }
}
