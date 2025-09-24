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
#include "BoundingBox.h"
#include "PureGLTF.h"

namespace pure {

// Invalid index constant for bounds references
constexpr std::size_t kInvalidBoundsIndex = static_cast<std::size_t>(-1);

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
    // Index into Model::bounds pool (combined bounding info for this geometry)
    std::size_t boundsIndex = kInvalidBoundsIndex;
    std::optional<std::vector<std::byte>> indicesData; // raw index buffer if present
    std::optional<GeometryIndicesMeta> indices; // metadata only
    // Optional double-precision decoded POSITION data (local space)
    std::optional<std::vector<glm::dvec3>> positions; 
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

    std::optional<glm::mat4> matrix; // when hasMatrix in the source
    std::optional<MeshNodeTransform> transform; // when using TRS in the source
    glm::mat4 world_matrix{1.0f};

    std::vector<std::size_t> subMeshes; // indices into Model::subMeshes

    // Index into Model::bounds pool for this node's world-space combined bounds
    std::size_t boundsIndex = kInvalidBoundsIndex;
};

struct Scene {
    std::string name;
    std::vector<std::size_t> nodes; // root node indices
    // Index into Model::bounds pool for this scene's world-space combined bounds
    std::size_t boundsIndex = kInvalidBoundsIndex;
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

    // Add or find a BoundingBox in the global pool; returns its index
    std::size_t internBounds(const BoundingBox& b);
};

// Convert from the original gltf::Model into the static format above.
Model ConvertFromGLTF(const gltf::Model& src);

} // namespace pure
