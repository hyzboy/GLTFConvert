#include "gltf/ToNodeTransform.h"

static glm::mat4 FastToGlmMat4(const fastgltf::math::fmat4x4 &mat)
{
    glm::mat4 m(1.0f);
    const float *d = mat.data();
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            m[c][r] = static_cast<float>(d[c * 4 + r]);
    return m;
}

static TRS FastToTRS(const fastgltf::TRS &src)
{
    TRS t;
    t.translation = glm::vec3(src.translation.x(), src.translation.y(), src.translation.z());
    t.rotation    = glm::quat(src.rotation.w(), src.rotation.x(), src.rotation.y(), src.rotation.z());
    t.scale       = glm::vec3(src.scale.x(), src.scale.y(), src.scale.z());
    return t;
}

NodeTransform ToNodeTransform(const std::variant<fastgltf::TRS, fastgltf::math::fmat4x4> &src)
{
    if (std::holds_alternative<fastgltf::TRS>(src))
    {
        return NodeTransform(FastToTRS(std::get<fastgltf::TRS>(src)));
    }
    return NodeTransform(FastToGlmMat4(std::get<fastgltf::math::fmat4x4>(src)));
}
