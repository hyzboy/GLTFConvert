#include "math/NodeTransform.h"
#include <cmath>

namespace
{
    inline glm::quat YUpToZUpRotationQuat()
    {
        return glm::angleAxis(glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));
    }

    inline glm::mat4 YUpToZUpRotationMat4()
    {
        return glm::mat4_cast(YUpToZUpRotationQuat());
    }

    bool isIdentityMatrix(const glm::mat4 &mat, float eps = 1e-6f) noexcept
    {
        for (int c = 0; c < 4; ++c)
        {
            for (int r = 0; r < 4; ++r)
            {
                const float expected = (c == r) ? 1.0f : 0.0f;
                if (std::fabs(mat[c][r] - expected) > eps)
                    return false;
            }
        }
        return true;
    }
}

NodeTransform::NodeTransform() noexcept : type(Type::None)
{
    m = glm::mat4(1.0f);
}

NodeTransform::NodeTransform(const glm::mat4 &mat) noexcept
{
    if (isIdentityMatrix(mat))
    {
        type = Type::None;
        m = glm::mat4(1.0f);
    }
    else
    {
        type = Type::Matrix;
        m = mat;
    }
}

NodeTransform::NodeTransform(const TRS &t) noexcept
{
    if (t.empty())
    {
        type = Type::None;
        m = glm::mat4(1.0f);
    }
    else
    {
        type = Type::TRS;
        trs = t;
    }
}

NodeTransform::NodeTransform(const NodeTransform &rhs) noexcept : type(rhs.type)
{
    switch (type)
    {
        case Type::Matrix: m = rhs.m; break;
        case Type::TRS:    trs = rhs.trs; break;
        case Type::None:   m = glm::mat4(1.0f); break;
    }
}

NodeTransform &NodeTransform::operator=(const NodeTransform &rhs) noexcept
{
    if (this == &rhs)
        return *this;

    type = rhs.type;
    switch (type)
    {
        case Type::Matrix: m = rhs.m; break;
        case Type::TRS:    trs = rhs.trs; break;
        case Type::None:   m = glm::mat4(1.0f); break;
    }
    return *this;
}

NodeTransform::NodeTransform(NodeTransform &&rhs) noexcept : type(rhs.type)
{
    switch (type)
    {
        case Type::Matrix: m = rhs.m; break;
        case Type::TRS:    trs = rhs.trs; break;
        case Type::None:   m = glm::mat4(1.0f); break;
    }
}

NodeTransform &NodeTransform::operator=(NodeTransform &&rhs) noexcept
{
    return (*this = static_cast<const NodeTransform &>(rhs));
}

void NodeTransform::setNone() noexcept
{
    type = Type::None;
    m = glm::mat4(1.0f);
}

void NodeTransform::setMatrix(const glm::mat4 &mat) noexcept
{
    if (isIdentityMatrix(mat))
    {
        type = Type::None;
        m = glm::mat4(1.0f);
    }
    else
    {
        type = Type::Matrix;
        m = mat;
    }
}

void NodeTransform::setTRS(const TRS &t) noexcept
{
    if (t.empty())
    {
        type = Type::None;
        m = glm::mat4(1.0f);
    }
    else
    {
        type = Type::TRS;
        trs = t;
    }
}

glm::mat4 NodeTransform::rawMat4() const
{
    switch (type)
    {
        case Type::Matrix: return m;
        case Type::TRS:    return trs.toMat4();
        case Type::None:   return glm::mat4(1.0f);
    }
    return glm::mat4(1.0f);
}

glm::mat4 NodeTransform::toZUpMat4() const
{
    return YUpToZUpRotationMat4() * rawMat4();
}

void NodeTransform::convertInPlaceYUpToZUp()
{
    if (type == Type::None)
        return;

    const glm::quat q = YUpToZUpRotationQuat();

    if (type == Type::Matrix)
    {
        m = glm::mat4_cast(q) * m;
    }
    else if (type == Type::TRS)
    {
        trs.translation = glm::vec3(glm::mat4_cast(q) * glm::vec4(trs.translation, 1.0f));
        trs.rotation = q * trs.rotation;
    }
}
