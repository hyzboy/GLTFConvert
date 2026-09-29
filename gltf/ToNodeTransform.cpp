#include "gltf/ToNodeTransform.h"

#include <cmath>
#include <iostream>
#include <glm/gtc/constants.hpp>

namespace
{
    /// Y-up → Z-up 的旋转：Rx(+90°)，与 gltf/import/GLTFOrientationPrimitives.cpp 对顶点用的同一个 R
    inline glm::mat4 YUpToZUpMat4()
    {
        return glm::mat4_cast(glm::angleAxis(glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f)));
    }

    inline glm::mat4 FastToGlmMat4(const fastgltf::math::fmat4x4 &mat)
    {
        glm::mat4 m(1.0f);
        const float *d = mat.data();
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                m[c][r] = static_cast<float>(d[c * 4 + r]);
        return m;
    }

    /// fastgltf TRS → glm 矩阵。注意 glTF 数组序是 [x,y,z,w]，glm 构造函数要 w 在前。
    inline glm::mat4 FastTRSToGlmMat4(const fastgltf::TRS &src)
    {
        const glm::vec3 t(src.translation.x(), src.translation.y(), src.translation.z());
        const glm::quat q(src.rotation.w(), src.rotation.x(), src.rotation.y(), src.rotation.z());
        const glm::vec3 s(src.scale.x(), src.scale.y(), src.scale.z());

        return glm::translate(glm::mat4(1.0f), t)
             * glm::mat4_cast(q)
             * glm::scale(glm::mat4(1.0f), s);
    }

    inline float MaxAbsDiff(const glm::mat4 &a, const glm::mat4 &b)
    {
        float d = 0.0f;
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
            {
                const float v = std::fabs(a[c][r] - b[c][r]);
                if (v > d)
                    d = v;
            }
        return d;
    }

    /**
     * 仿射矩阵 → TRS（**镜像感知**）。
     *
     * 与库函数的差别（本函数存在的唯一理由）：
     *  - 缩放取列模长；若 det<0（镜像）**负号只放进缩放**，列本身保持原样：
     *    列/负缩放 = -列/|列|，恰好把这一列翻回右手系，得到合法旋转。
     *    ⚠ 反例（文档中给出的写法是错的）：同时取负 scale.x 与列 c0 ⇒ 双重翻转 ⇒ 旋转列整体反号。
     *  - 单轴退化为零（"scale=0 隐藏部件"这类资产）时不失败：该列对矩阵没有贡献，用另外两轴
     *    的叉积补一个正交基即可精确重建；只有**两轴以上退化**才返回 false。
     */
    bool DecomposeAffine(const glm::mat4 &m, glm::vec3 &out_t, glm::quat &out_r, glm::vec3 &out_s)
    {
        out_t = glm::vec3(m[3]);

        glm::vec3 axis[3] = { glm::vec3(m[0]), glm::vec3(m[1]), glm::vec3(m[2]) };
        glm::vec3 s(glm::length(axis[0]), glm::length(axis[1]), glm::length(axis[2]));

        if (glm::determinant(glm::mat3(axis[0], axis[1], axis[2])) < 0.0f)
            s.x = -s.x;                       // 镜像：负号进缩放，列保持原样

        constexpr float kZeroEps = 1e-12f;

        int degenerate = -1;
        int degenerate_count = 0;

        for (int i = 0; i < 3; ++i)
        {
            if (std::fabs(s[i]) > kZeroEps)
            {
                axis[i] /= s[i];
                continue;
            }

            s[i] = 0.0f;
            degenerate = i;
            ++degenerate_count;
        }

        if (degenerate_count > 1)
            return false;                     // 两轴以上退化：旋转已无从确定

        if (degenerate_count == 1)
            axis[degenerate] = glm::cross(axis[(degenerate + 1) % 3], axis[(degenerate + 2) % 3]);

        out_r = glm::normalize(glm::quat_cast(glm::mat3(axis[0], axis[1], axis[2])));
        out_s = s;

        return true;
    }

    /// TRS 保真判据：重建矩阵与原矩阵的允许残差（超过即判定"无法用 TRS 表示"）
    constexpr float kTrsFidelityEpsilon = 1e-4f;
}//namespace

bool ToNodeTransform(const std::variant<fastgltf::TRS, fastgltf::math::fmat4x4> &src,
                     std::size_t node_index,
                     const std::string &node_name,
                     NodeTransform &out_transform)
{
    // 1) 原始局部矩阵（Y-up）
    const glm::mat4 raw = std::holds_alternative<fastgltf::TRS>(src)
                        ? FastTRSToGlmMat4(std::get<fastgltf::TRS>(src))
                        : FastToGlmMat4(std::get<fastgltf::math::fmat4x4>(src));

    // 2) Y-up → Z-up：M' = R · M · R⁻¹（相似变换 / 共轭）
    //
    //    这是与"图元顶点被旋转 v' = R·v"配对的唯一正确写法：M'·v' = R·(M·v)，全链路恰好一次 R。
    //    ⚠ 不要改回 "t'=R·t、r'=q·r·q⁻¹、s 原样保留" 的手写代数：R·diag(s)·R⁻¹ 在 R=Rx90 时是
    //    "y/z 互换"的对角阵而不是原 s，非均匀缩放 + 旋转的节点会被歪掉
    //    （实测 TRS 节点 S=(2,0.5,1)、绕 X 40°：偏差 0.383，并一路影响 bounds/渲染）。
    const glm::mat4 R   = YUpToZUpMat4();
    const glm::mat4 zup = R * raw * glm::transpose(R);

    // 3) 镜像感知分解
    glm::vec3 t, s;
    glm::quat q;

    if (!DecomposeAffine(zup, t, q, s))
    {
        std::cerr << "[Import] 错误：node " << node_index << " (\"" << node_name
                  << "\") 的局部变换含两个以上退化轴（矩阵已塌成平面/线），无法分解为 TRS\n";
        return false;
    }

    // 4) 保真自检：TRS 能表达的范围 = 无剪切 + 最多单轴退化。
    //    剪切是**结构性不可表示**的（TRS 结构里没有这一自由度），因此这里 fail-fast：
    //    宁可转换失败，也不产出一个"看起来对、实际与源文件不一致"的模型。
    const glm::mat4 rebuilt = glm::translate(glm::mat4(1.0f), t)
                            * glm::mat4_cast(q)
                            * glm::scale(glm::mat4(1.0f), s);

    const float residual = MaxAbsDiff(rebuilt, zup);
    if (residual > kTrsFidelityEpsilon)
    {
        std::cerr << "[Import] 错误：node " << node_index << " (\"" << node_name
                  << "\") 的局部变换无法用 TRS 表示（含剪切/畸形矩阵），残差 " << residual
                  << " > " << kTrsFidelityEpsilon
                  << "；请在 DCC 中对该节点清除剪切（或烘焙矩阵）后重新导出\n";
        return false;
    }

    out_transform = NodeTransform(TRS{ t, q, s });

    return true;
}
