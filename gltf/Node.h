#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstddef>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

struct GLTFNode
{
    std::string name;
    std::optional<std::size_t> mesh;
    std::vector<std::size_t> children;

    bool hasMatrix=false; // true -> use matrix, false -> use TRS
    glm::vec3 translation{ 0.0f };
    glm::quat rotation{ 1.0f,0.0f,0.0f,0.0f };
    glm::vec3 scale{ 1.0f };
    glm::mat4 matrix{ 1.0f };

    glm::mat4 worldMatrix{ 1.0f };

    glm::mat4 localMatrix() const; // implemented in PureGLTF.cpp (now Model impl file)
};
