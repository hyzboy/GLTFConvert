#pragma once

#include <string>
#include <vector>
#include <optional>
#include <array>
#include <cstddef>
#include <limits>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include "AABB.h"

namespace gltf {

// struct AABB moved to global namespace in AABB.h

struct Geometry {
    std::string mode; // e.g. TRIANGLES
    struct Attribute {
        std::string name;
        std::size_t count = 0;
        std::string componentType; // e.g. FLOAT
        std::string type; // e.g. VEC3
        std::vector<std::byte> data; // raw data as-is
        // Source glTF accessor index (for dedup by accessor identity)
        std::optional<std::size_t> accessorIndex;
    };
    std::vector<Attribute> attributes;
    std::optional<std::vector<std::byte>> indices;
    // metadata for indices
    std::optional<std::size_t> indexCount; // number of indices
    std::optional<std::string> indexComponentType; // e.g. UNSIGNED_SHORT
    // Source glTF indices accessor index (for dedup)
    std::optional<std::size_t> indicesAccessorIndex;

    ::AABB localAABB; // computed from POSITION if present
};

struct Primitive {
    Geometry geometry; // geometry payload

    // material index from glTF primitive (kept for building SubMesh during export)
    std::optional<std::size_t> material;
};

struct Material {
    std::string name;
};

struct Mesh {
    std::string name;
    std::vector<std::size_t> primitives; // indices into Model::primitives
};

struct Node {
    std::string name;
    std::optional<std::size_t> mesh;
    std::vector<std::size_t> children;

    bool hasMatrix = false; // true -> use matrix, false -> use TRS
    glm::dvec3 translation{0.0};
    glm::dquat rotation{1.0,0.0,0.0,0.0}; // w,x,y,z in glm constructor order (w first)
    glm::dvec3 scale{1.0};
    glm::dmat4 matrix{1.0};

    glm::dmat4 worldMatrix{1.0};

    glm::dmat4 localMatrix() const {
        if (hasMatrix) return matrix;
        glm::dmat4 m(1.0);
        m = glm::translate(m, translation);
        m *= glm::mat4_cast(rotation);
        m = glm::scale(m, scale);
        return m;
    }
};

struct Scene {
    std::string name;
    std::vector<std::size_t> nodes; // root node indices
    ::AABB worldAABB; // computed
};

struct Model {
    std::string source;

    std::vector<Primitive> primitives;
    std::vector<Mesh> meshes;
    std::vector<Node> nodes;
    std::vector<Scene> scenes;

    // materials from the asset
    std::vector<Material> materials;

    // compute per-node world matrices across all scenes
    void computeWorldMatrices();
    // compute per-scene AABBs using node world matrices and primitive localAABBs
    void computeSceneAABBs();
    // Apply global basis change to Z-up (rotate +90deg around X) to all nodes, then recompute derived data
    void convertToZUp();
};

} // namespace gltf
