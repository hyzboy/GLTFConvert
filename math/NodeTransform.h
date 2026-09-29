#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include "math/TRS.h"

/**
 * NodeTransform —— 节点局部变换，只有两种状态：None（单位变换）与 TRS。
 *
 * **没有 Matrix 状态**：局部矩阵一律在导入边界（gltf/ToNodeTransform.cpp）分解为 TRS。
 * 原因：
 *   ① 引擎侧（ULRE）的局部真源本来就是 TRS（TransformDataStorage 的 positions/rotations/scales）；
 *   ② 矩阵状态无法在导入期完成"镜像/剪切"这类合法性判定，只会把畸形数据带进后续全链路
 *      （实测：含 det<0 的 matrix 节点在不做镜像判定时，四元数会变成非单位值、镜像被静默丢弃）。
 * 需要矩阵时用 rawMat4()（TRS 展开）。
 */
struct NodeTransform
{
    enum class Type : uint8_t { None, TRS };

    Type type { Type::None };
    TRS  trs;

    NodeTransform() noexcept;
    explicit NodeTransform(const TRS &t) noexcept;

    NodeTransform(const NodeTransform &rhs) noexcept = default;
    NodeTransform &operator=(const NodeTransform &rhs) noexcept = default;
    NodeTransform(NodeTransform &&rhs) noexcept = default;
    NodeTransform &operator=(NodeTransform &&rhs) noexcept = default;

    // State queries
    bool isNone() const noexcept { return type == Type::None; }
    bool isTRS()  const noexcept { return type == Type::TRS; }

    // Mutators
    void setNone() noexcept;
    void setTRS(const TRS &t) noexcept;

    /// TRS 展开为 4x4 矩阵（None → 单位矩阵）
    glm::mat4 rawMat4() const;
};
