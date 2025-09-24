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
#include "PureGLTF.h"

namespace pure {

struct Material {
    std::string name;
};

struct GeometryAttribute {
    int64_t id = 0; // index in the attributes array
    std::string name;
    std::size_t count = 0;
    std::string componentType; // e.g. FLOAT
    std::string type; // e.g. VEC3
    std::vector<std::byte> data; // raw attribute data
};

struct GeometryIndicesMeta {
    std::size_t count = 0;
    std::string componentType; // e.g. UNSIGNED_SHORT
};

struct Geometry {
    std::string mode; // e.g. TRIANGLES
    std::vector<GeometryAttribute> attributes; // includes raw data
    ::AABB aabb; // empty() means null in exported JSON
    std::optional<std::vector<std::byte>> indicesData; // raw index buffer if present
    std::optional<GeometryIndicesMeta> indices; // metadata only
};

struct SubMesh {
    std::size_t geometry = static_cast<std::size_t>(-1); // index into Model::geometry
    std::optional<std::size_t> material; // index into Model::materials or null
};

// Restore TRS-based transform representation (float for real-time use)
struct MeshNodeTransform {
    glm::vec3 translation{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f}; // w,x,y,z
    glm::vec3 scale{1.0f};
};

struct MeshNode {
    std::string name;
    std::vector<std::size_t> children;

    std::optional<glm::dmat4> matrix; // when hasMatrix in the source
    std::optional<MeshNodeTransform> transform; // when using TRS in the source
    glm::mat4 world_matrix{1.0f};

    std::vector<std::size_t> subMeshes; // indices into Model::subMeshes

    ::AABB aabb; // world-space AABB for this node (empty if no geometry)
};

struct Scene {
    std::string name;
    std::vector<std::size_t> nodes; // root node indices
    ::AABB aabb; // empty() means null in exported JSON
};

struct Model {
    std::string gltf_source;

    std::vector<Material> materials;
    std::vector<Scene> scenes;
    std::vector<MeshNode> mesh_nodes;

    std::vector<Geometry> geometry; // unique geometry set
    std::vector<SubMesh> subMeshes; // one-to-one with glTF primitives
};

// Convert from the original gltf::Model into the static format above.
Model ConvertFromGLTF(const gltf::Model& src);

} // namespace pure
