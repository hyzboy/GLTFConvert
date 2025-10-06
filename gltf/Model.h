#pragma once

#include <string>
#include <vector>
#include <cstddef>
#include "gltf/Primitive.h"
#include "gltf/Mesh.h"
#include "gltf/Node.h"
#include "gltf/Scene.h"
#include "gltf/Material.h"

struct GLTFModel
{
    std::string source;

    std::vector<GLTFPrimitive> primitives;
    std::vector<GLTFMesh> meshes;
    std::vector<GLTFNode> nodes;
    std::vector<GLTFScene> scenes;
    std::vector<GLTFMaterial> materials;
};
