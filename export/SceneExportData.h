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
    struct SceneNodeExport
    {
        int32_t originalIndex{-1};
        std::string name;
        glm::mat4 localMatrix{1.0f};
        std::vector<int32_t> subMeshes;   // subMesh indices
        std::vector<int32_t> children;    // child node indices
    };

    struct SceneSubMeshExport
    {
        int32_t originalIndex{-1};
        int32_t geometryIndex{-1};
        std::string geometryFile;                 // baseName.i.geometry
        std::optional<int32_t> materialIndex;     // material index
    };

    struct SceneMaterialExport
    {
        int32_t originalIndex{-1};
        std::string name;
    };

    struct SceneGeometryExport
    {
        int32_t originalIndex{-1};
        std::string file; // baseName.i.geometry
    };

    struct SceneExportData
    {
        std::string sceneName;
        std::vector<SceneNodeExport>      nodes;
        std::vector<SceneSubMeshExport>   subMeshes;
        std::vector<SceneMaterialExport>  materials;
        std::vector<SceneGeometryExport>  geometries;
    };

    // Build data for a scene (geometryBaseName is used to compose geometry file names baseName.i.geometry)
    SceneExportData BuildSceneExportData(const pure::Model &model, std::size_t sceneIndex, const std::string &geometryBaseName);

    // Serialization (JSON / binary pack). Implemented in cpp.
    bool WriteSceneJson(const SceneExportData &data, const std::filesystem::path &filePath);
    bool WriteScenePack(const SceneExportData &data, const std::filesystem::path &filePath);
}
