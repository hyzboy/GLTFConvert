#pragma once

#include <variant>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include "math/TRS.h"
#include "fastgltf/types.hpp"

struct NodeTransform
{
    bool is_matrix=true;

    union
    {
        glm::mat4 m;
        TRS trs;
    };

    // 赋值自 fastgltf 的变体（TRS 或矩阵）
    const NodeTransform &operator = (const std::variant<fastgltf::TRS, fastgltf::math::fmat4x4> &n)
    {
        if(std::holds_alternative<fastgltf::TRS>(n))
        {
            is_matrix=false;
            trs = std::get<fastgltf::TRS>(n);
        }
        else
        {
            is_matrix=true;
            const auto &mat=std::get<fastgltf::math::fmat4x4>(n);
            m = toMat4(mat);
        }
        return *this; // 修复缺失的返回
    }

public:

    NodeTransform()
    {
        is_matrix=false;
        trs.init();
    }

    glm::mat4 rawMat4() const { return is_matrix ? m : trs.toMat4(); }

    // glTF(Y-up) -> Z-up 旋转（绕 X 轴 +90°）
    static glm::quat YUpToZUpRotationQuat() { return glm::angleAxis(glm::half_pi<float>(), glm::vec3(1.0f,0.0f,0.0f)); }
    static glm::mat4 YUpToZUpRotationMat4() { return glm::mat4_cast(YUpToZUpRotationQuat()); }

    glm::mat4 toZUpMat4() const { return YUpToZUpRotationMat4() * rawMat4(); }

    void convertInPlaceYUpToZUp() {
        const glm::quat q = YUpToZUpRotationQuat();
        if(is_matrix) {
            m = glm::mat4_cast(q) * m;
        } else {
            trs.translation = glm::vec3(glm::mat4_cast(q) * glm::vec4(trs.translation, 1.0f));
            trs.rotation = q * trs.rotation;
        }
    }

    static glm::quat ZUpToYUpRotationQuat() { return glm::angleAxis(-glm::half_pi<float>(), glm::vec3(1.0f,0.0f,0.0f)); }
    glm::mat4 toYUpMat4() const { return glm::mat4_cast(ZUpToYUpRotationQuat()) * rawMat4(); }
};
