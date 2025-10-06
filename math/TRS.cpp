#include "TRS.h"
#include <fastgltf/core.hpp>

TRS& TRS::operator=(const fastgltf::TRS& rhs) {
    translation = glm::vec3(rhs.translation.x(), rhs.translation.y(), rhs.translation.z());
    rotation = glm::quat(rhs.rotation.w(), rhs.rotation.x(), rhs.rotation.y(), rhs.rotation.z());
    scale = glm::vec3(rhs.scale.x(), rhs.scale.y(), rhs.scale.z());
    return *this;
}
