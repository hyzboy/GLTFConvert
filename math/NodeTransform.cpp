#include "math/NodeTransform.h"

NodeTransform::NodeTransform() noexcept : type(Type::None)
{
}

NodeTransform::NodeTransform(const TRS &t) noexcept : type(Type::None)
{
    setTRS(t);
}

void NodeTransform::setNone() noexcept
{
    type = Type::None;
    trs.init();
}

void NodeTransform::setTRS(const TRS &t) noexcept
{
    if (t.empty())
    {
        setNone();
        return;
    }

    type = Type::TRS;
    trs = t;
}

glm::mat4 NodeTransform::rawMat4() const
{
    return (type == Type::TRS) ? trs.toMat4() : glm::mat4(1.0f);
}
