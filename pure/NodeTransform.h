#pragma once

#include <glm/glm.hpp>
#include "math/TRS.h"
#include "fastgltf/types.hpp"

namespace pure
{
    struct NodeTransform
    {
        bool is_matrix;

        union
        {
            glm::mat4 m;
            TRS trs;
        };

        const NodeTransform &operator = (const std::variant<fastgltf::TRS, fastgltf::math::fmat4x4> &n)
        {
            if(std::holds_alternative<fastgltf::TRS>(n))
            {
                is_matrix=false;

                trs=std::get<fastgltf::TRS>(n);
            }
            else
            {
                is_matrix=true;
                
                const auto &mat=std::get<fastgltf::math::fmat4x4>(n);

                m=toMat4(mat);
            }
        }
    };
}
