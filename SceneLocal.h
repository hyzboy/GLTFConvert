#pragma once

#include <string>
#include <vector>
#include <cstddef>
#include <glm/glm.hpp>
#include <unordered_map>

#include "Geometry.h" // for kInvalidBoundsIndex

// forward declarations to avoid heavy includes
struct BoundingBox; // global

namespace pure {

struct MeshNode;            // from StaticMesh.h
struct SubMesh;             // from StaticMesh.h
struct MeshNodeTransform;   // from StaticMesh.h
struct Model;               // from StaticMesh.h

// Scene-local remapped data for a single scene
struct SceneLocal {
    std::string name;
    std::vector<MeshNode> nodes;                 // scene-local nodes with remapped indices
    std::vector<int32_t> roots;                  // root node indices into `nodes`
    std::vector<SubMesh> subMeshes;              // scene-local subMesh pool (geometry indices are global)
    std::vector<glm::mat4> matrixData;           // scene-local unique matrices
    std::vector<MeshNodeTransform> trsPool;      // scene-local TRS
    std::vector<BoundingBox> bounds;             // scene-local bounds pool
    int32_t sceneBoundsIndex = kInvalidBoundsIndex; // index into `bounds` or kInvalidBoundsIndex

    // Added for binary export name table construction
    std::vector<std::string> nameList;                       // consolidated names (scene, geometry refs, node names)
    std::unordered_map<std::string, uint32_t> nameMap;       // intern map
    std::vector<uint32_t> subMeshNameIndices;                // indices into nameList for each subMesh geometry file name
    std::vector<uint32_t> nodeNameIndices;                   // indices into nameList for each node name
};

// Build scene-local data (remap node/submesh/matrix/TRS/bounds indices to compact scene-local pools)
SceneLocal BuildSceneLocal(const Model& sm, int32_t sceneIndex);

} // namespace pure
