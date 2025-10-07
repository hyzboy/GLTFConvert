#pragma once

#include <vector>
#include <unordered_map>
#include <cstdint>

namespace pure { struct Model; struct Scene; }

namespace exporters
{
    // Indices collected from a scene before remapping.
    struct CollectedIndices
    {
        std::vector<int32_t> nodes;
        std::vector<int32_t> subMeshes;
        std::vector<int32_t> materials;
        std::vector<int32_t> geometries;
    };

    // Maps original model indices -> contiguous scene-local indices.
    struct RemapTables
    {
        std::unordered_map<int32_t,int32_t> nodeRemap;
        std::unordered_map<int32_t,int32_t> subMeshRemap;
        std::unordered_map<int32_t,int32_t> materialRemap;
        std::unordered_map<int32_t,int32_t> geometryRemap;
    };

    // Collect all original indices referenced by the scene hierarchy.
    CollectedIndices CollectSceneIndices(const pure::Model &model, const pure::Scene &scene);

    // Build remapping tables for each category.
    RemapTables BuildRemapTables(const CollectedIndices &ci);
}
