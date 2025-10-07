#include "SceneExportMaterials.h"

#include "pure/Model.h"
#include <filesystem>

namespace exporters
{
    void BuildMaterials(const pure::Model &model,
                        const CollectedIndices &ci,
                        std::unordered_map<std::string, int32_t> &nameToIndex,
                        SceneExportData &outData)
    {
        (void)nameToIndex; // no longer needed for materials
        outData.materials.reserve(ci.materials.size());

        std::string baseName;
        if (!model.gltf_source.empty())
        {
            baseName = std::filesystem::path(model.gltf_source).stem().string();
        }
        else
        {
            baseName = "scene"; // fallback
        }

        for (int32_t originalMat : ci.materials)
        {
            const auto &mat = model.materials[originalMat];
            SceneMaterialExport me;
            me.originalIndex = originalMat;
            if (!mat.name.empty())
                me.file = baseName + "." + mat.name + ".material";
            else
                me.file = baseName + ".material." + std::to_string(originalMat); // fallback unique name
            outData.materials.push_back(std::move(me));
        }
    }
}
