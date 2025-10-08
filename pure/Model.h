#pragma once
#include <string>
#include <vector>
#include "pure/Material.h"
#include "pure/MeshNode.h"
#include "pure/Scene.h"
#include "pure/Geometry.h"
#include "pure/SubMesh.h"
#include "pure/Image.h"
#include "pure/Texture.h"
#include "pure/Sampler.h"

namespace pure
{
    struct Model
    {
        std::string gltf_source;
        std::vector<Material> materials;
        std::vector<Scene> scenes;
        std::vector<MeshNode> mesh_nodes;
        std::vector<Geometry> geometry;
        std::vector<SubMesh> subMeshes;
        std::vector<Image> images; // images
        std::vector<Texture> textures; // textures
        std::vector<Sampler> samplers; // samplers
    };
}
