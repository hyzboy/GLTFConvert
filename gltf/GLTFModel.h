#pragma once

#include <string>
#include <vector>
#include <cstddef>
#include "gltf/GLTFPrimitive.h"
#include "gltf/GLTFMesh.h"
#include "gltf/GLTFNode.h"
#include "gltf/GLTFScene.h"
#include "gltf/GLTFMaterial.h"
#include "gltf/GLTFImage.h"

struct GLTFModel
{
    std::string source;

    std::vector<GLTFPrimitive> primitives;
    std::vector<GLTFMesh> meshes;
    std::vector<GLTFNode> nodes;
    std::vector<GLTFScene> scenes;
    std::vector<GLTFMaterial> materials;
    std::vector<GLTFImage> images; // new: imported images
};
