#pragma once
#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <glm/glm.hpp>
#include "math/AABB.h"
#include "math/OBB.h"
#include "pure/Material.h"
#include "pure/MeshNode.h"
#include "pure/Scene.h"
#include "pure/Geometry.h"
#include "pure/SubMesh.h"
#include "math/TRS.h"
#include "pure/BoundsIndex.h"
#include "gltf/GLTFModel.h"

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
        std::vector<BoundingBox> bounds;
        std::vector<glm::mat4> matrixData;
        std::vector<TRS> trsPool;
        int32_t internBounds(const BoundingBox &b);
        int32_t internTRS(const TRS &t);
        int32_t internMatrix(const glm::mat4 &m);
    };

    glm::mat4 GetNodeWorldMatrix(const Model &m,const MeshNode &n);
    glm::mat4 GetNodeLocalMatrix(const Model &m,const MeshNode &n);
    const std::optional<TRS> &GetNodeTRS(const Model &m,const MeshNode &n);

    Model ConvertFromGLTF(const GLTFModel &src);
}
