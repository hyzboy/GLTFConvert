#pragma once

#include <cstddef>
#include <vector>
#include <glm/glm.hpp>

#include "pure/ModelCore.h"

namespace pure
{
    // 基于 subMeshes 与 world_matrix 计算单个节点的世界空间包围体（写入全局 bounds 池并赋索引）
    void ComputeMeshNodeBounds(pure::Model &model,int32_t nodeIndex);
    void ComputeAllMeshNodeBounds(pure::Model &model);

    // 按场景根节点遍历收集点，计算场景 OBB/Sphere（AABB 沿用现有的），写入全局 bounds 池并赋索引
    void ComputeSceneBounds(pure::Model &model,pure::Scene &scene);
    void ComputeAllSceneBounds(pure::Model &model);

    // 新接口：读取节点世界矩阵（通过统一变换池）
    inline glm::mat4 GetWorldMatrix(const pure::Model &m,const pure::MeshNode &n)
    {
        return GetNodeWorldMatrix(m,n);
    }

} // namespace pure
