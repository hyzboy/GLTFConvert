#pragma once

#include <string>
#include <vector>
#include <cstddef>
#include <glm/glm.hpp>

#include "Geometry.h" // for kInvalidBoundsIndex

// forward declarations to avoid heavy includes
struct BoundingBox; // global

namespace pure {

struct MeshNode;            // from StaticMesh.h
struct SubMesh;             // from StaticMesh.h
struct MatrixEntry;         // from StaticMesh.h
struct MeshNodeTransform;   // from StaticMesh.h
struct Model;               // from StaticMesh.h

// Scene-local remapped data for a single scene
struct SceneLocal {
    std::string name;
    std::vector<MeshNode> nodes;                 // scene-local nodes with remapped indices
    std::vector<int32_t> roots;                  // root node indices into `nodes`
    std::vector<SubMesh> subMeshes;              // scene-local subMesh pool (geometry indices are global)
    std::vector<MatrixEntry> matrixEntryPool;    // scene-local matrix entries
    std::vector<glm::mat4> matrixData;           // scene-local unique matrices
    std::vector<MeshNodeTransform> trsPool;      // scene-local TRS
    std::vector<BoundingBox> bounds;             // scene-local bounds pool
    int32_t sceneBoundsIndex = kInvalidBoundsIndex; // index into `bounds` or kInvalidBoundsIndex
};

// Build scene-local data (remap node/submesh/matrix/TRS/bounds indices to compact scene-local pools)
SceneLocal BuildSceneLocal(const Model& sm, int32_t sceneIndex);

} // namespace pure
