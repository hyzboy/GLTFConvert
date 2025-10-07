#pragma once

#include <string>
#include <vector>
#include <cstddef>
#include <glm/glm.hpp>
#include <unordered_map>
#include <filesystem>

#include "pure/BoundsIndex.h"
#include "pure/MeshNode.h"
#include "pure/SubMesh.h"
#include "math/TRS.h"
#include "pure/ModelCore.h"

struct BoundingVolumes; // forward declaration

namespace pure
{

    // SceneExporter: combines scene-local data building & binary export
    class SceneExporter
    {
    public:
        std::string name;
        std::vector<MeshNode> nodes;
        std::vector<int32_t> roots;
        std::vector<SubMesh> subMeshes;
        std::vector<glm::mat4> matrixData;
        std::vector<TRS> trsPool;
        std::vector<BoundingVolumes> bounds;
        int32_t sceneBoundsIndex=kInvalidBoundsIndex;
        std::vector<std::string> nameList;
        std::unordered_map<std::string,uint32_t> nameMap;
        std::vector<uint32_t> subMeshNameIndices;
        std::vector<uint32_t> nodeNameIndices;
        static SceneExporter Build(const Model &sm,int32_t sceneIndex);
        bool WriteSceneBinary(const std::filesystem::path &sceneDir,const std::string &baseName) const;
    };

} // namespace pure
