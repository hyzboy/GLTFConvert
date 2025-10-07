#include "gltf/ToNodeTransform.h"
#include "math/TRS.h"

NodeTransform ToNodeTransform(const std::variant<fastgltf::TRS, fastgltf::math::fmat4x4> &src)
{
    if (std::holds_alternative<fastgltf::TRS>(src))
    {
        TRS t; t = std::get<fastgltf::TRS>(src);
        return NodeTransform(t); // constructor folds identity -> None
    }
    const auto &mat = std::get<fastgltf::math::fmat4x4>(src);
    return NodeTransform(toMat4(mat)); // constructor folds identity -> None
}
