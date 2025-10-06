#pragma once

#include "math/OBB.h"
#include "math/BoundingSphere.h"
#include <ostream>
#include <vector>

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

    template<typename T>
    bool fromPoints(const std::vector<T> &pts) // now float precision input
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
