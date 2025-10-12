#pragma once
#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include "pure/Material.h"
#include "pure/Scene.h"
#include "pure/Geometry.h"
#include "pure/Primitive.h" // now defines Primitive
#include "pure/Image.h"
#include "pure/Texture.h"
#include "pure/Sampler.h"
#include "pure/Mesh.h"
#include "pure/Node.h"

namespace pure
{
    struct Model
    {
        std::string model_source;
        std::vector<std::unique_ptr<Material>> materials;
        std::vector<Scene> scenes;
        std::vector<Node> nodes;        // nodes
        std::vector<Mesh> meshes;       // meshes (list of primitive indices)
        std::vector<Geometry> geometry; // unique geometry buffers
        std::vector<Primitive> primitives; // formerly subMeshes
        std::vector<Image> images;
        std::vector<Texture> textures;
        std::vector<Sampler> samplers;

        std::string GetBaseName() const
        {
            if (model_source.empty()) return std::string("scene");
            return std::filesystem::path(model_source).stem().string();
        }
    };
}
