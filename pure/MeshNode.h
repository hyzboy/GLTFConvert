#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <glm/glm.hpp>
#include "pure/BoundsIndex.h"
#include "math/NodeTransform.h"

namespace pure
{
    struct MeshNode
    {
    public: // 原始数据

        std::string name;

        NodeTransform node_transform;

        std::vector<int32_t> children;
        std::vector<int32_t> subMeshes;

    public: // 中间计算数据

//        BoundingBox bounds;

    public: // 保存用数据

        int32_t localMatrixIndexPlusOne=0;
        int32_t worldMatrixIndexPlusOne=0;
        int32_t trsIndexPlusOne=0;
        int32_t boundsIndex=kInvalidBoundsIndex;

    };
}
