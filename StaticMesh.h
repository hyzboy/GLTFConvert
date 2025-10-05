#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "math/AABB.h"
#include "math/OBB.h"
#include "gltf/Model.h"
#include "pure/Geometry.h"
#include "SubMesh.h"
#include "math/MeshNodeTransform.h"

namespace pure {

struct Material { std::string name; };

struct MeshNode {
    std::string name;
    std::vector<int32_t> children;

    // Direct indices (plus one) into Model::matrixData. 0 => identity/not stored.
    int32_t localMatrixIndexPlusOne = 0;
    int32_t worldMatrixIndexPlusOne = 0;

    int32_t trsIndexPlusOne = 0; // refers to Model::trsPool (plus one, 0 => identity/not stored)

    std::vector<int32_t> subMeshes; // indices into Model::subMeshes
    int32_t boundsIndex = kInvalidBoundsIndex; // world-space combined bounds index
};

struct Scene {
    std::string name;
    std::vector<int32_t> nodes; // root node indices
    int32_t boundsIndex = kInvalidBoundsIndex; // world-space combined bounds index
};

struct Model {
    std::string gltf_source;

    std::vector<Material> materials;
    std::vector<Scene> scenes;
    std::vector<MeshNode> mesh_nodes;

    std::vector<Geometry> geometry; // unique geometry set
    std::vector<SubMesh> subMeshes; // one-to-one with glTF primitives

    std::vector<BoundingBox> bounds; // global pool of bounds

    // Deduped raw matrices pool
    std::vector<glm::mat4> matrixData; // unique matrices (local/world)

    std::vector<MeshNodeTransform> trsPool; // deduped TRS

    int32_t internBounds(const BoundingBox& b);
    int32_t internTRS(const MeshNodeTransform& t);
    // Dedup a single matrix; returns its index (not plus one). Identity matrices are not stored and return -1.
    int32_t internMatrix(const glm::mat4& m);
};

inline glm::mat4 GetNodeWorldMatrix(const Model& m, const MeshNode& n) {
    if (n.worldMatrixIndexPlusOne == 0) return glm::mat4(1.0f);
    const auto idx = n.worldMatrixIndexPlusOne - 1;
    return m.matrixData[static_cast<std::size_t>(idx)];
}

inline glm::mat4 GetNodeLocalMatrix(const Model& m, const MeshNode& n) {
    if (n.localMatrixIndexPlusOne == 0) return glm::mat4(1.0f);
    const auto idx = n.localMatrixIndexPlusOne - 1;
    return m.matrixData[static_cast<std::size_t>(idx)];
}

inline const std::optional<MeshNodeTransform>& GetNodeTRS(const Model& m, const MeshNode& n) {
    if (n.trsIndexPlusOne == 0) {
        static const std::optional<MeshNodeTransform> kEmpty{};
        return kEmpty;
    }
    const auto idx = n.trsIndexPlusOne - 1;
    struct Holder { std::optional<MeshNodeTransform> opt; };
    static thread_local Holder holder;
    holder.opt = m.trsPool[static_cast<std::size_t>(idx)];
    return holder.opt;
}

Model ConvertFromGLTF(const GLTFModel& src);

} // namespace pure
