#pragma once

#include <cstddef>
#include <vector>

#include "StaticMesh.h"

namespace pure {

// 从 glTF 几何体解码 POSITION 并填充 pure::Geometry 的本地空间包围体（AABB/OBB/Sphere）到全局 bounds 池
void ComputeGeometryBoundsFromGLTF(pure::Model& model, const gltf::Geometry& srcGeom, pure::Geometry& dstGeom);

// 基于 subMeshes 与 world_matrix 计算单个节点的世界空间包围体（写入全局 bounds 池并赋索引）
void ComputeMeshNodeBounds(pure::Model& model, std::size_t nodeIndex);
void ComputeAllMeshNodeBounds(pure::Model& model);

// 按场景根节点遍历收集点，计算场景 OBB/Sphere（AABB 沿用现有的），写入全局 bounds 池并赋索引
void ComputeSceneBounds(pure::Model& model, pure::Scene& scene);
void ComputeAllSceneBounds(pure::Model& model);

// 新接口：读取节点世界矩阵（通过统一变换池）
inline glm::mat4 GetWorldMatrix(const pure::Model& m, const pure::MeshNode& n) {
    return GetNodeWorldMatrix(m, n);
}

} // namespace pure
