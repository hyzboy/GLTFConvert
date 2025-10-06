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
BoundingSphere SphereFromPoints(const std::vector<glm::vec4> &pts); // vec4 version (w component ignored)

// Combined bounding volumes holding AABB, OBB, and Sphere (all single precision)
struct BoundingVolumes
{
    AABB aabb; // axis-aligned bounding box (float)
    OBB  obb;  // oriented bounding box (float)
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

    bool fromPoints(const std::vector<glm::vec3> &pts) // now float precision input
    {
        reset();
        if(pts.empty()) return false;

        aabb=AABB::fromPoints(pts);
        obb=OBB::fromPointsMinVolume(pts);
        sphere=SphereFromPoints(pts);

        return true;
    }
};

void Write(std::ostream &os,const BoundingVolumes &volumes);
