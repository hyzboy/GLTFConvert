#pragma once

#include <string>
#include <vector>
#include <cstddef>
#include <glm/glm.hpp>
#include <unordered_map>
#include <filesystem>

#include "pure/BoundsIndex.h" // for kInvalidBoundsIndex

struct BoundingBox; // forward declaration

namespace pure {

struct MeshNode;            // from StaticMesh.h
struct SubMesh;             // from StaticMesh.h
struct MeshNodeTransform;   // from StaticMesh.h
struct Model;               // from StaticMesh.h

// SceneExporter: combines scene-local data building & binary export
class SceneExporter {
public:
    std::string name;
    std::vector<MeshNode> nodes;                 // scene-local nodes with remapped indices
    std::vector<int32_t> roots;                  // root node indices into `nodes`
    std::vector<SubMesh> subMeshes;              // scene-local subMesh pool (geometry indices are global)
    std::vector<glm::mat4> matrixData;           // scene-local unique matrices
    std::vector<MeshNodeTransform> trsPool;      // scene-local TRS
    std::vector<BoundingBox> bounds;             // scene-local bounds pool
    int32_t sceneBoundsIndex = kInvalidBoundsIndex; // index into `bounds` or kInvalidBoundsIndex

    // Name table data for binary export
    std::vector<std::string> nameList;                       // consolidated names (scene, geometry refs, node names)
    std::unordered_map<std::string, uint32_t> nameMap;       // intern map
    std::vector<uint32_t> subMeshNameIndices;                // indices into nameList for each subMesh geometry file name
    std::vector<uint32_t> nodeNameIndices;                   // indices into nameList for each node name

    // Build (replaces BuildSceneLocal)
    static SceneExporter Build(const Model& sm, int32_t sceneIndex);

    // Write scene binary pack (renamed from WriteBinary)
    bool WriteSceneBinary(const std::filesystem::path& sceneDir, const std::string& baseName) const;
};

} // namespace pure
