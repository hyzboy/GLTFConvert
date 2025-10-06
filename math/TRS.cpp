#include "TRS.h"
#include <fastgltf/core.hpp>

TRS &TRS::operator=(const fastgltf::TRS &rhs)
{
    translation=glm::vec3(rhs.translation.x(),rhs.translation.y(),rhs.translation.z());
    rotation=glm::quat(rhs.rotation.w(),rhs.rotation.x(),rhs.rotation.y(),rhs.rotation.z());
    scale=glm::vec3(rhs.scale.x(),rhs.scale.y(),rhs.scale.z());
    return *this;
}

glm::mat4 toMat4(const fastgltf::math::fmat4x4 &mat)
{
    glm::mat4 m(1.0);
    const float *d=mat.data();
    // column-major fill
    for(int c=0; c<4; ++c)
        for(int r=0; r<4; ++r)
            m[c][r]=static_cast<double>(d[c*4+r]);

    return m;
}
