#pragma once

#include <glm/glm.hpp>
#include <cmath>
#include <vector>
#include <limits>
#include <algorithm>
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

    void reset();
    bool empty() const;
    static OBB fromAABB(const AABB& aabb);
    static OBB fromTransform(const glm::dmat4& m, const glm::dvec3& localHalfSize);
    OBB transformed(const glm::dmat4& m) const;
    AABB toAABB() const;
    void corners(glm::dvec3 out[8]) const;
    static OBB fromPointsMinVolume(const glm::dvec3* points, size_t count,
                                   double coarseStepDeg = 15.0,
                                   double fineStepDeg = 3.0,
                                   double ultraStepDeg = 0.5);
    static OBB fromPointsMinVolume(const std::vector<glm::dvec3>& points,
                                   double coarseStepDeg = 15.0,
                                   double fineStepDeg = 3.0,
                                   double ultraStepDeg = 0.5);
};
