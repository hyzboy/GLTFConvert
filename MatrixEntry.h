#pragma once

#include <glm/glm.hpp>

namespace pure {

struct MatrixEntry {
    glm::mat4 local{1.0f};
    glm::mat4 world{1.0f};
};

} // namespace pure
