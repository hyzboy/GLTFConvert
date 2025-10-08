#include "SceneExportData.h"
#include "SceneExportCollect.h"
#include "SceneExportTransforms.h"
#include "SceneExportNames.h"
#include "SceneExportNodes.h"
#include "SceneExportSubMeshes.h"
#include "SceneExportMaterials.h"
#include "SceneExportGeometries.h"
#include "SceneExportComputeBounds.h"

#include <unordered_map>
#include "pure/Model.h"
#include "pure/Scene.h"

namespace exporters
{
    SceneExportData BuildSceneExportData(const pure::Model &model,
                                         std::size_t sceneIndex,
                                         const std::string &geometryBaseName)
    {
        SceneExportData data;
        if (sceneIndex >= model.scenes.size())
            return data;

        const auto &scene = model.scenes[sceneIndex];

        // 1. Collect indices
        CollectedIndices collected = CollectSceneIndices(model, scene);

        // 2. Remap tables
        RemapTables remap = BuildRemapTables(collected);

        // 3. World matrices
        std::vector<glm::mat4> worldMatrices = ComputeWorldMatrices(model, scene);

        // 4. Name table + scene name
        std::unordered_map<std::string,int32_t> nameToIndex;
        nameToIndex.reserve(collected.nodes.size() + collected.materials.size() + 1);
        data.sceneNameIndex = GetOrAddName(nameToIndex, data.nameTable, scene.name);

        // 5. Identity matrix entry (optional convenience)
        GetOrAddMatrix(data.matrixTable, glm::mat4(1.0f));

        // 6. Nodes
        BuildNodes(model, collected, remap, worldMatrices, nameToIndex, data);

        // 7. Bounds
        ComputeBounds(model, worldMatrices, data);

        // 8. SubMeshes / Materials / Geometries
        BuildSubMeshes(collected, geometryBaseName, data);
        BuildMaterials(model, collected, nameToIndex, data);
        BuildGeometries(collected, geometryBaseName, data);

        return data;
    }
}
