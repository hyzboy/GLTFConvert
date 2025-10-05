#pragma once

#include "math/AABB.h"
#include "math/OBB.h"
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

// Combined bounding info holding both an AABB and an OBB (now single precision primitives, sphere center float)
struct BoundingBox
{
    AABB aabb; // axis-aligned (float)
    OBB  obb;  // oriented (float)
    BoundingSphere sphere; // bounding sphere (float)

    void reset()
    {
        aabb.reset();
        obb.reset();
        sphere.reset();
    }

    bool emptyAABB() const { return aabb.empty(); }
    bool emptyOBB()  const { return obb.empty(); }
    bool emptySphere() const { return sphere.empty(); }
};

void Write(std::ostream &os,const BoundingBox &b);
