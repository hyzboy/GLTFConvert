#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstddef>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "math/AABB.h"
#include "common/VKFormat.h"
#include "common/IndexType.h"
#include "common/PrimitiveType.h"

namespace gltf {

struct Geometry {
    std::string mode; // e.g. TRIANGLES

    PrimitiveType primitiveType = PrimitiveType::Triangles;

    struct Attribute {
        std::string name;
        std::size_t count = 0;
        std::string componentType; // e.g. FLOAT
        std::string type; // e.g. VEC3

        VkFormat format;

        std::vector<std::byte> data; // raw data as-is
        std::optional<std::size_t> accessorIndex; // Source glTF accessor index
    };
    std::vector<Attribute> attributes;
    std::optional<std::vector<std::byte>> indices; // raw index data
    std::optional<std::size_t> indexCount;         // number of indices
    std::optional<std::string> indexComponentType; // e.g. UNSIGNED_SHORT

    IndexType indexType = IndexType::ERR;
    std::optional<std::size_t> indicesAccessorIndex; // source accessor index

    ::AABB localAABB; // computed from POSITION if present
};

struct Primitive {
    Geometry geometry; // geometry payload
    std::optional<std::size_t> material; // material index
};

struct Material { std::string name; };

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
    glm::dquat rotation{1.0,0.0,0.0,0.0};
    glm::dvec3 scale{1.0};
    glm::dmat4 matrix{1.0};

    glm::dmat4 worldMatrix{1.0};

    glm::dmat4 localMatrix() const; // moved out-of-line to cpp to reduce header includes
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
    std::vector<Material> materials;

    void computeWorldMatrices(); // compute per-node world matrices across all scenes
    void computeSceneAABBs();    // compute per-scene AABBs
};

} // namespace gltf
