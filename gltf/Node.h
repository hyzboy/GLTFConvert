#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstddef>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace gltf {

struct GLTFNode {
    std::string name;
    std::optional<std::size_t> mesh;
    std::vector<std::size_t> children;

    bool hasMatrix = false; // true -> use matrix, false -> use TRS
    glm::dvec3 translation{0.0};
    glm::dquat rotation{1.0,0.0,0.0,0.0};
    glm::dvec3 scale{1.0};
    glm::dmat4 matrix{1.0};

    glm::dmat4 worldMatrix{1.0};

    glm::dmat4 localMatrix() const; // implemented in PureGLTF.cpp (now Model impl file)
};

} // namespace gltf
