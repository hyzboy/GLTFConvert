#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "AABB.h"
#include "OBB.h"
#include "PureGLTF.h"
#include "Geometry.h"
#include "SubMesh.h"
#include "MeshNodeTransform.h"
#include "MatrixEntry.h"

namespace pure {


struct Material {
    std::string name;
};

struct MeshNode {
    std::string name;
    std::vector<int32_t> children;

    // Separate indices into pools (plus one). 0 indicates identity/empty and not stored in pool.
    int32_t matrixIndexPlusOne = 0; // refers to Model::matrixPool
    int32_t trsIndexPlusOne = 0;    // refers to Model::trsPool

    std::vector<int32_t> subMeshes; // indices into Model::subMeshes (now int32_t)

    // Index into Model::bounds pool for this node's world-space combined bounds
    int32_t boundsIndex = kInvalidBoundsIndex;
};

struct Scene {
    std::string name;
    std::vector<int32_t> nodes; // root node indices
    // Index into Model::bounds pool for this scene's world-space combined bounds
    int32_t boundsIndex = kInvalidBoundsIndex;
};

struct Model {
    std::string gltf_source;

    std::vector<Material> materials;
    std::vector<Scene> scenes;
    std::vector<MeshNode> mesh_nodes;

    std::vector<Geometry> geometry; // unique geometry set
    std::vector<SubMesh> subMeshes; // one-to-one with glTF primitives

    // Global pool of unique bounding boxes (AABB + OBB + Sphere)
    std::vector<BoundingBox> bounds;

    // Separate transform pools
    std::vector<MatrixEntry> matrixPool;              // deduped local/world matrices
    std::vector<MeshNodeTransform> trsPool;           // deduped TRS

    // Add or find a BoundingBox in the global pool; returns its index
    int32_t internBounds(const BoundingBox& b);

    // Add or find in TRS pool; returns index
    int32_t internTRS(const MeshNodeTransform& t);

    // Add or find in matrix pool (local+world pair); returns index
    int32_t internMatrix(const MatrixEntry& m);
};

// Helpers to access matrices from the matrix pool
inline glm::mat4 GetNodeWorldMatrix(const Model& m, const MeshNode& n) {
    if (n.matrixIndexPlusOne == 0) return glm::mat4(1.0f);
    const auto idx = n.matrixIndexPlusOne - 1; // int32_t
    return m.matrixPool[static_cast<std::size_t>(idx)].world;
}

inline glm::mat4 GetNodeLocalMatrix(const Model& m, const MeshNode& n) {
    if (n.matrixIndexPlusOne == 0) return glm::mat4(1.0f);
    const auto idx = n.matrixIndexPlusOne - 1;
    return m.matrixPool[static_cast<std::size_t>(idx)].local;
}

inline const std::optional<MeshNodeTransform>& GetNodeTRS(const Model& m, const MeshNode& n) {
    if (n.trsIndexPlusOne == 0) {
        static const std::optional<MeshNodeTransform> kEmpty{}; // no TRS
        return kEmpty;
    }
    const auto idx = n.trsIndexPlusOne - 1;
    // Provide reference to an optional wrapper (thread-local to be safe)
    struct Holder { std::optional<MeshNodeTransform> opt; };
    static thread_local Holder holder;
    holder.opt = m.trsPool[static_cast<std::size_t>(idx)];
    return holder.opt;
}

// Convert from the original gltf::Model into the static format above.
Model ConvertFromGLTF(const gltf::Model& src);

} // namespace pure
