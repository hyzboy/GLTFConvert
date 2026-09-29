#pragma once

#include <cstddef>
#include <string>
#include <variant>
#include "math/NodeTransform.h"
#include "fastgltf/types.hpp"
#include "fastgltf/math.hpp"

/**
 * 由 fastgltf 的节点变换（TRS 或 matrix）生成引擎侧 NodeTransform。
 *
 * 导入边界上一次性做对两件事：
 *  1. Y-up → Z-up：M' = R · M · R⁻¹（R = Rx(+90°)）——与图元顶点旋转 v' = R·v **配对**，
 *     于是全链路恰好应用一次 R：M'·v' = R·(M·v)；
 *  2. 镜像感知的仿射分解：负缩放（det<0）保留在缩放分量里，不被静默丢弃。
 *
 * @return true  = 节点变换已无损分解为 TRS，out_transform 可直接使用（NodeTransform 内部会自动
 *                 把单位变换收敛为 Type::None）；
 *         false = 该节点局部变换**无法用 TRS 表示**（含剪切 / 两轴以上退化）——调用方必须中止
 *                 转换（fail-fast），不要带半成品继续。失败原因与节点名已打印到 stderr。
 *
 * node_index / node_name 仅用于错误定位。
 */
bool ToNodeTransform(const std::variant<fastgltf::TRS, fastgltf::math::fmat4x4> &src,
                     std::size_t node_index,
                     const std::string &node_name,
                     NodeTransform &out_transform);
