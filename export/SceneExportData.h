#pragma once

#include <filesystem>
#include <vector>
#include <string>
#include <optional>
#include <cstdint>
#include <glm/glm.hpp>
#include "math/BoundingVolumes.h"
#include "math/TRS.h"

namespace pure { struct Model; }

namespace exporters
{
    struct SceneNodeExport
    {
        int32_t originalIndex { -1 };
        int32_t nameIndex { -1 };
        int32_t localMatrixIndex { -1 };
        int32_t worldMatrixIndex { -1 };
        int32_t trsIndex { -1 };
        int32_t boundsIndex { -1 };
        std::vector<int32_t> primitives;   // remapped scene-local primitive indices
        std::vector<int32_t> children;     // remapped scene-local node indices
    };

    struct ScenePrimitiveExport
    {
        int32_t originalIndex { -1 };          // index in source model (primitive index)
        int32_t geometryIndex { -1 };          // remapped scene-local geometry index
        std::string geometryFile;              // baseName.<originalGeometryIndex>.geometry
        std::optional<int32_t> materialIndex;  // remapped scene-local material index
    };

    struct SceneMaterialExport
    {
        int32_t originalIndex { -1 };
        std::string file;
    };

    struct SceneGeometryExport
    {
        int32_t originalIndex { -1 };
        std::string file;
    };

    struct SceneExportData
    {
        std::vector<std::string> nameTable;
        int32_t sceneNameIndex { -1 };

        std::vector<TRS>       trsTable;
        std::vector<glm::mat4> matrixTable;

        std::vector<BoundingVolumes> boundsTable;
        int32_t sceneBoundsIndex { -1 };

        std::vector<SceneNodeExport>      nodes;
        std::vector<ScenePrimitiveExport> primitives;  // formerly subMeshes
        std::vector<SceneMaterialExport>  materials;
        std::vector<SceneGeometryExport>  geometries;
    };

    SceneExportData BuildSceneExportData(const pure::Model &model,
                                         std::size_t sceneIndex,
                                         const std::string &geometryBaseName);

    bool WriteSceneJson(const SceneExportData &data, const std::filesystem::path &filePath);
    bool WriteScenePack(const SceneExportData &data, const std::filesystem::path &filePath);
}
