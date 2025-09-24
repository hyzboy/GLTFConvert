#pragma once

#include "AABB.h"
#include "OBB.h"

struct BoundingSphere {
    glm::dvec3 center{0.0};
    double radius{-1.0}; // radius <= 0 means empty
    void reset() { center = glm::dvec3(0.0); radius = -1.0; }
    bool empty() const { return radius <= 0.0; }
};

BoundingSphere SphereFromAABB(const AABB &a);
BoundingSphere SphereFromPoints(const std::vector<glm::dvec3> &pts);

// Combined bounding info holding both an AABB and an OBB in double precision
struct BoundingBox {
    AABB aabb; // axis-aligned
    OBB  obb;  // oriented
    BoundingSphere sphere; // bounding sphere

    void reset() {
        aabb.reset();
        obb.reset();
        sphere.reset();
    }

    bool emptyAABB() const { return aabb.empty(); }
    bool emptyOBB()  const { return obb.empty(); }
    bool emptySphere() const { return sphere.empty(); }
};
