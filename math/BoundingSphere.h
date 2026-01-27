#pragma once

#include "math/AABB.h"
#include <ostream>
#include <vector>

struct BoundingSphere
{
    glm::vec3 center{ 0.0f };
    float radius{ -1.0f }; // radius <= 0 means empty
    void reset() { center=glm::vec3(0.0f); radius=-1.0f; }
    bool empty() const { return radius<=0.0f; }
};

BoundingSphere SphereFromAABB(const AABB &a);
BoundingSphere SphereFromPoints(const std::vector<glm::vec3> &pts); // now float precision input
BoundingSphere SphereFromPoints(const std::vector<glm::vec4> &pts); // vec4 version (w component ignored)
