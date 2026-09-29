#include "SceneExportData.h"
#include "SceneExportCollect.h"
#include "SceneExportTransforms.h"
#include "SceneExportNames.h"
#include "SceneExportNodes.h"
#include "SceneExportPrimitives.h"
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

        // 3. World matrices（仅导出内部：算 world AABB；产物只存 TRS）
        std::vector<glm::mat4> worldMatrices = ComputeWorldMatrices(model, scene);

        // 4. 名字表 + 场景名
        std::unordered_map<std::string,int32_t> nameToIndex;
        nameToIndex.reserve(collected.nodes.size() + collected.materials.size() + 1);
        data.sceneNameIndex = GetOrAddName(nameToIndex, data.nameTable, scene.name);

        // 4. 节点（局部变换只写 TRS；world 矩阵由消费者从 TRS 组合，产物不存矩阵）
        BuildNodes(model, collected, remap, nameToIndex, data);

        // 6b. Root nodes (scene.nodes remapped to scene-local indices)
        for (int32_t original : scene.nodes)
        {
            auto it = remap.nodeRemap.find(original);
            if (it != remap.nodeRemap.end())
                data.rootNodes.push_back(it->second);
        }

        // 7. Bounds
        ComputeBounds(model, worldMatrices, data);

        // 8. Primitives / Materials / Geometries
        BuildPrimitivesExport(collected, data);
        BuildMaterials(model, collected, nameToIndex, data);
        BuildGeometries(collected, geometryBaseName, static_cast<int32_t>(model.geometry.size()), data);

        // 9. Link primitive -> scene-local geometryIndex and materialIndex.
        //    BuildPrimitivesExport can't do this itself (no model access), so we patch here.
        //    几何文件名一并取自几何表（单一真源 ⇒ 包内名字必然等于磁盘上的文件）。
        {
            std::unordered_map<int32_t, int32_t> geoOrigToLocal;
            geoOrigToLocal.reserve(data.geometries.size());
            for (int32_t _j = 0; _j < static_cast<int32_t>(data.geometries.size()); ++_j)
                geoOrigToLocal[data.geometries[_j].originalIndex] = _j;

            for (auto &pe : data.primitives)
            {
                if (pe.originalIndex >= 0 && pe.originalIndex < static_cast<int32_t>(model.primitives.size()))
                {
                    const auto &mp = model.primitives[pe.originalIndex];
                    const auto geoIt = geoOrigToLocal.find(mp.geometry);
                    if (geoIt != geoOrigToLocal.end())
                    {
                        pe.geometryIndex = geoIt->second;
                        pe.geometryFile  = data.geometries[geoIt->second].file;
                    }
                    if (mp.material.has_value())
                    {
                        const auto matIt = remap.materialRemap.find(*mp.material);
                        if (matIt != remap.materialRemap.end())
                            pe.materialIndex = matIt->second;
                    }
                }
            }
        }

        return data;
    }
}
