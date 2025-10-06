#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace fastgltf { struct TRS; }

// Restore TRS-based transform representation (float for real-time use)
struct TRS
{
    glm::vec3 translation{ 0.0f };
    glm::quat rotation{ 1.0f,0.0f,0.0f,0.0f }; // w,x,y,z
    glm::vec3 scale{ 1.0f };

    // 判断是否为空（单位变换）
    bool empty() const {
        return translation == glm::vec3(0.0f)
            && rotation == glm::quat(1.0f,0.0f,0.0f,0.0f)
            && scale == glm::vec3(1.0f);
    }

    // 支持从fastgltf::TRS赋值
    TRS& operator=(const fastgltf::TRS& rhs);

    // 转换为glm::mat4
    glm::mat4 toMat4() const {
        glm::mat4 m = glm::mat4(1.0f);
        m = glm::translate(m, translation);
        m *= glm::mat4_cast(rotation);
        m = glm::scale(m, scale);
        return m;
    }
};
