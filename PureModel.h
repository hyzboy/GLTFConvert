#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "PureGLTF.h" // for gltf::Model
#include "Geometry.h"
#include "BoundingBox.h"
#include "SubMesh.h"
#include "MeshNodeTransform.h"
#include "MatrixEntry.h"
#include "PureMaterial.h"
#include "MeshNode.h"
#include "Scene.h"

namespace pure {

// PureModel: static representation built from glTF, contains deduped geometry, pools, and bounds
struct PureModel {
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
    std::size_t internBounds(const BoundingBox& b);

    // Add or find in TRS pool; returns index
    std::size_t internTRS(const MeshNodeTransform& t);

    // Add or find in matrix pool (local+world pair); returns index
    std::size_t internMatrix(const MatrixEntry& m);
};

// Helpers to access matrices from the matrix pool
inline glm::mat4 GetNodeWorldMatrix(const PureModel& m, const MeshNode& n) {
    if (n.matrixIndexPlusOne == 0) return glm::mat4(1.0f);
    const auto idx = n.matrixIndexPlusOne - 1;
    return m.matrixPool[idx].world;
}

inline glm::mat4 GetNodeLocalMatrix(const PureModel& m, const MeshNode& n) {
    if (n.matrixIndexPlusOne == 0) return glm::mat4(1.0f);
    const auto idx = n.matrixIndexPlusOne - 1;
    return m.matrixPool[idx].local;
}

inline const std::optional<MeshNodeTransform>& GetNodeTRS(const PureModel& m, const MeshNode& n) {
    if (n.trsIndexPlusOne == 0) {
        static const std::optional<MeshNodeTransform> kEmpty{}; // no TRS
        return kEmpty;
    }
    const auto idx = n.trsIndexPlusOne - 1;
    // Provide reference to an optional wrapper (thread-local to be safe)
    struct Holder { std::optional<MeshNodeTransform> opt; };
    static thread_local Holder holder;
    holder.opt = m.trsPool[idx];
    return holder.opt;
}

// Convert from the original gltf::Model into the static format above.
PureModel ConvertFromGLTF(const gltf::Model& src);

} // namespace pure
