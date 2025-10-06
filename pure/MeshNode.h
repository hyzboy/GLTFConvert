#pragma once
#include <string>
#include <vector>
#include <cstdint>
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
    };
}
