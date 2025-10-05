#pragma once

#include <string>
#include <vector>
#include <cstddef>
#include "gltf/Primitive.h"
#include "gltf/Mesh.h"
#include "gltf/Node.h"
#include "gltf/Scene.h"
#include "gltf/Material.h"

namespace gltf {

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
