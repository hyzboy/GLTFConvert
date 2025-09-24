#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include "AABB.h"

// Oriented Bounding Box in double precision, global namespace
struct OBB {
    // Center of the box
    glm::dvec3 center{0.0};
    // Orthonormal basis axes (intended to be unit length). Columns of the orientation matrix.
    glm::dvec3 axisX{1.0, 0.0, 0.0};
    glm::dvec3 axisY{0.0, 1.0, 0.0};
    glm::dvec3 axisZ{0.0, 0.0, 1.0};
    // Half sizes along each axis (non-negative). If any component <= 0, the OBB is considered empty.
    glm::dvec3 halfSize{0.0};

    void reset() {
        center = glm::dvec3(0.0);
        axisX = glm::dvec3(1.0, 0.0, 0.0);
        axisY = glm::dvec3(0.0, 1.0, 0.0);
        axisZ = glm::dvec3(0.0, 0.0, 1.0);
        halfSize = glm::dvec3(0.0);
    }

    bool empty() const {
        return (halfSize.x <= 0.0) || (halfSize.y <= 0.0) || (halfSize.z <= 0.0);
    }

    // Build an OBB from an AABB (axes aligned with world axes)
    static OBB fromAABB(const AABB& aabb) {
        OBB obb;
        if (aabb.empty()) return obb;
        obb.center = (aabb.min + aabb.max) * 0.5;
        obb.axisX = glm::dvec3(1.0, 0.0, 0.0);
        obb.axisY = glm::dvec3(0.0, 1.0, 0.0);
        obb.axisZ = glm::dvec3(0.0, 0.0, 1.0);
        obb.halfSize = (aabb.max - aabb.min) * 0.5;
        return obb;
    }

    // Construct an OBB from a transform matrix and half sizes in the local box space.
    // The transform's linear part provides orientation and scale; translation sets the center.
    static OBB fromTransform(const glm::dmat4& m, const glm::dvec3& localHalfSize) {
        OBB obb;
        const glm::dmat3 L(m); // upper-left 3x3 linear part
        const glm::dvec3 col0 = glm::dvec3(L[0]);
        const glm::dvec3 col1 = glm::dvec3(L[1]);
        const glm::dvec3 col2 = glm::dvec3(L[2]);

        const double len0 = glm::length(col0);
        const double len1 = glm::length(col1);
        const double len2 = glm::length(col2);

        obb.center = glm::dvec3(m[3]); // translation (column 3)
        obb.axisX = (len0 > 0.0) ? (col0 / len0) : glm::dvec3(1,0,0);
        obb.axisY = (len1 > 0.0) ? (col1 / len1) : glm::dvec3(0,1,0);
        obb.axisZ = (len2 > 0.0) ? (col2 / len2) : glm::dvec3(0,0,1);
        obb.halfSize = glm::dvec3(localHalfSize.x * len0, localHalfSize.y * len1, localHalfSize.z * len2);
        return obb;
    }

    // Apply an affine transform to the OBB. For general linear transforms, axes are re-normalized
    // and half sizes are scaled by the stretch along each transformed axis.
    OBB transformed(const glm::dmat4& m) const {
        if (empty()) return *this;
        OBB out;
        out.center = glm::dvec3(m * glm::dvec4(center, 1.0));
        const glm::dmat3 L(m);

        const glm::dvec3 v0 = L * axisX;
        const glm::dvec3 v1 = L * axisY;
        const glm::dvec3 v2 = L * axisZ;

        const double l0 = glm::length(v0);
        const double l1 = glm::length(v1);
        const double l2 = glm::length(v2);

        out.axisX = (l0 > 0.0) ? (v0 / l0) : axisX;
        out.axisY = (l1 > 0.0) ? (v1 / l1) : axisY;
        out.axisZ = (l2 > 0.0) ? (v2 / l2) : axisZ;
        out.halfSize = glm::dvec3(halfSize.x * l0, halfSize.y * l1, halfSize.z * l2);
        return out;
    }

    // Convert this OBB to a tight AABB in the same space.
    // Uses the well-known formula: extents = |R| * halfSize, where R has columns axisX, axisY, axisZ.
    AABB toAABB() const {
        AABB aabb;
        if (empty()) {
            aabb.reset(); // leave empty
            return aabb;
        }
        const glm::dvec3 ax = glm::abs(axisX);
        const glm::dvec3 ay = glm::abs(axisY);
        const glm::dvec3 az = glm::abs(axisZ);
        // Component-wise combination to get world extents
        const glm::dvec3 e = ax * halfSize.x + ay * halfSize.y + az * halfSize.z;
        aabb.min = center - e;
        aabb.max = center + e;
        return aabb;
    }

    // Fill an array with the 8 corners of the box, if needed by clients.
    void corners(glm::dvec3 out[8]) const {
        const glm::dvec3 ex = axisX * halfSize.x;
        const glm::dvec3 ey = axisY * halfSize.y;
        const glm::dvec3 ez = axisZ * halfSize.z;
        out[0] = center - ex - ey - ez;
        out[1] = center + ex - ey - ez;
        out[2] = center - ex + ey - ez;
        out[3] = center + ex + ey - ez;
        out[4] = center - ex - ey + ez;
        out[5] = center + ex - ey + ez;
        out[6] = center - ex + ey + ez;
        out[7] = center + ex + ey + ez;
    }
};
