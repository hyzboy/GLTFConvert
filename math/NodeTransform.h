#pragma once

#include <glm/glm.hpp>
#include "math/TRS.h"

/**
 * NodeTransform —— 节点局部变换，唯一真源就是一个 TRS。
 *
 * 设计要点（两条都是"不留第二真源"）：
 *   ① **没有 Matrix 状态**：局部矩阵一律在导入边界（gltf/ToNodeTransform.cpp）分解为 TRS。
 *      引擎侧（ULRE）的局部真源本来就是 TRS；而矩阵状态无法在导入期完成"镜像/剪切"这类
 *      合法性判定，只会把畸形数据带进后续全链路（实测：含 det<0 的 matrix 节点在不做镜像判定时，
 *      四元数会变成非单位值、镜像被静默丢弃）。
 *   ② **没有 Type 状态**："是不是单位变换"由 `trs.empty()` 自己回答（TRS 自带 empty()）。
 *      以前那份 `enum class Type { None, TRS }` 只是 `!trs.empty()` 的复制品 —— 同一事实两个真源，
 *      且 isNone() 零调用者。需要判定单位变换的地方直接写 `trs.empty()`。
 *
 * 需要矩阵时用 rawMat4()（TRS 展开；单位变换 → 单位矩阵）。
 */
struct NodeTransform
{
    TRS trs;

    NodeTransform() noexcept = default;
    explicit NodeTransform(const TRS &t) noexcept : trs(t) {}

    /// TRS 展开为 4x4 矩阵（单位变换 → 单位矩阵）
    glm::mat4 rawMat4() const { return trs.toMat4(); }
};
