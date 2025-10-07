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
    // All name strings are deduplicated into SceneExportData::nameTable.
    // Name index fields store -1 when original name was empty.

    struct SceneNodeExport
    {
        int32_t originalIndex { -1 };          // index in source model
        int32_t nameIndex { -1 };              // index into nameTable (-1 if no name)
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
        int32_t nameIndex { -1 };              // index into nameTable (-1 if no name)
    };

    struct SceneGeometryExport
    {
        int32_t originalIndex { -1 };          // index in source model
        std::string file;                      // baseName.<originalGeometryIndex>.geometry
    };

    struct SceneExportData
    {
        // Name table shared by scene, nodes, materials.
        std::vector<std::string> nameTable;    // deduplicated names
        int32_t sceneNameIndex { -1 };         // index into nameTable (-1 if empty)

        std::vector<SceneNodeExport>      nodes;       // index is scene-local node id
        std::vector<SceneSubMeshExport>   subMeshes;   // index is scene-local subMesh id
        std::vector<SceneMaterialExport>  materials;   // index is scene-local material id
        std::vector<SceneGeometryExport>  geometries;  // index is scene-local geometry id
    };

    SceneExportData BuildSceneExportData(const pure::Model &model,
                                         std::size_t sceneIndex,
                                         const std::string &geometryBaseName);

    bool WriteSceneJson(const SceneExportData &data, const std::filesystem::path &filePath);
    bool WriteScenePack(const SceneExportData &data, const std::filesystem::path &filePath);
}
