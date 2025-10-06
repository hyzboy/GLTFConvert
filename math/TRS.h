#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace pure
{

    // Restore TRS-based transform representation (float for real-time use)
    struct TRS
    {
        glm::vec3 translation{ 0.0f };
        glm::quat rotation{ 1.0f,0.0f,0.0f,0.0f }; // w,x,y,z
        glm::vec3 scale{ 1.0f };
    };

} // namespace pure
