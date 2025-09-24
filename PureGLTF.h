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

namespace puregltf {

struct AABB {
    glm::dvec3 min{ std::numeric_limits<double>::infinity() };
    glm::dvec3 max{ -std::numeric_limits<double>::infinity() };

    void reset() {
        min = { std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity() };
        max = { -std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity() };
    }

    bool empty() const {
        return !(min.x <= max.x && min.y <= max.y && min.z <= max.z);
    }

    void include(const glm::dvec3& p) {
        min = glm::min(min, p);
        max = glm::max(max, p);
    }

    void merge(const AABB& other) {
        if (other.empty()) return;
        include(other.min);
        include(other.max);
    }

    AABB transformed(const glm::dmat4& m) const {
        if (empty()) return *this;
        // transform 8 corners and recompute
        const glm::dvec3 c[8] = {
            {min.x, min.y, min.z}, {max.x, min.y, min.z}, {min.x, max.y, min.z}, {min.x, min.y, max.z},
            {max.x, max.y, min.z}, {max.x, min.y, max.z}, {min.x, max.y, max.z}, {max.x, max.y, max.z}
        };
        AABB out; out.reset();
        for (auto& p : c) {
            glm::dvec4 tp = m * glm::dvec4(p, 1.0);
            out.include(glm::dvec3(tp));
        }
        return out;
    }
};

struct Primitive {
    std::string mode; // e.g. TRIANGLES
    struct Attribute {
        std::string name;
        std::size_t count = 0;
        std::string componentType; // e.g. FLOAT
        std::string type; // e.g. VEC3
        std::vector<std::byte> data; // raw data as-is
    };
    std::vector<Attribute> attributes;
    std::optional<std::vector<std::byte>> indices;

    AABB localAABB; // computed from POSITION if present
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
    AABB worldAABB; // computed
};

struct Model {
    std::string source;

    std::vector<Primitive> primitives;
    std::vector<Mesh> meshes;
    std::vector<Node> nodes;
    std::vector<Scene> scenes;

    // compute per-node world matrices across all scenes
    void computeWorldMatrices();
    // compute per-scene AABBs using node world matrices and primitive localAABBs
    void computeSceneAABBs();
    // Apply global basis change to Z-up (rotate +90deg around X) to all nodes, then recompute derived data
    void convertToZUp();
};

} // namespace puregltf
