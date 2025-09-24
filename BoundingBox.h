#pragma once

#include "AABB.h"
#include "OBB.h"

// Combined bounding info holding both an AABB and an OBB in double precision
struct BoundingBox {
    AABB aabb; // axis-aligned
    OBB  obb;  // oriented

    void reset() {
        aabb.reset();
        obb.reset();
    }

    bool emptyAABB() const { return aabb.empty(); }
    bool emptyOBB()  const { return obb.empty(); }
};
