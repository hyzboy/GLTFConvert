#pragma once

#include <filesystem>
#include <vector>
#include <string>
#include <optional>
#include <cstdint>
#include <glm/glm.hpp>

namespace pure { struct Model; }

namespace exporters
{
    // NOTE: All index reference fields (subMeshes / children / geometryIndex / materialIndex)
    // are remapped to be contiguous, scene-local indices in the exported data.
    // The original indices from the source pure::Model are preserved in the
    // 'originalIndex' member of each exported element for traceability.

    struct SceneNodeExport
    {
        int32_t originalIndex { -1 };          // index in source model
        std::string name;
        glm::mat4 localMatrix { 1.0f };
        std::vector<int32_t> subMeshes;        // remapped scene-local subMesh indices
        std::vector<int32_t> children;         // remapped scene-local node indices
    };

    struct SceneSubMeshExport
    {
        int32_t originalIndex { -1 };          // index in source model
        int32_t geometryIndex { -1 };          // remapped scene-local geometry index
        std::string geometryFile;              // baseName.<originalGeometryIndex>.geometry
        std::optional<int32_t> materialIndex;  // remapped scene-local material index
    };

    struct SceneMaterialExport
    {
        int32_t originalIndex { -1 };          // index in source model
        std::string name;
    };

    struct SceneGeometryExport
    {
        int32_t originalIndex { -1 };          // index in source model
        std::string file;                      // baseName.<originalGeometryIndex>.geometry
    };

    struct SceneExportData
    {
        std::string sceneName;
        std::vector<SceneNodeExport>      nodes;       // index is scene-local node id
        std::vector<SceneSubMeshExport>   subMeshes;   // index is scene-local subMesh id
        std::vector<SceneMaterialExport>  materials;   // index is scene-local material id
        std::vector<SceneGeometryExport>  geometries;  // index is scene-local geometry id
    };

    // Build data for a scene (geometryBaseName is used to compose geometry file names baseName.i.geometry)
    SceneExportData BuildSceneExportData(const pure::Model &model,
                                         std::size_t sceneIndex,
                                         const std::string &geometryBaseName);

    // Serialization (JSON / binary pack). Implemented in cpp.
    bool WriteSceneJson(const SceneExportData &data, const std::filesystem::path &filePath);
    bool WriteScenePack(const SceneExportData &data, const std::filesystem::path &filePath);
}
