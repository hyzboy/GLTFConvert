#pragma once

#include <glm/glm.hpp>
#include <cmath>
#include <vector>
#include <limits>
#include <algorithm>
#include "math/AABB.h"

// Oriented Bounding Box in single precision
struct OBB
{
    // Center of the box
    glm::vec3 center{0.0f};
    // Orthonormal basis axes (intended to be unit length). Columns of the orientation matrix.
    glm::vec3 axisX{1.0f,0.0f,0.0f};
    glm::vec3 axisY{0.0f,1.0f,0.0f};
    glm::vec3 axisZ{0.0f,0.0f,1.0f};
    // Half sizes along each axis (non-negative). If any component <= 0, the OBB is considered empty.
    glm::vec3 halfSize{0.0f};

    void reset();
    bool empty() const;
    static OBB fromAABB(const AABB &aabb); // AABB still double precision; converted internally
    static OBB fromTransform(const glm::mat4 &m,const glm::vec3 &localHalfSize);
    OBB transformed(const glm::mat4 &m) const;
    AABB toAABB() const; // returns double-precision AABB
    void corners(glm::vec3 out[8]) const;
    static OBB fromPointsMinVolume(const glm::vec3 *points,size_t count,
                                   float coarseStepDeg=15.0f,
                                   float fineStepDeg=3.0f,
                                   float ultraStepDeg=0.5f);
    static OBB fromPointsMinVolume(const std::vector<glm::vec3> &points,
                                   float coarseStepDeg=15.0f,
                                   float fineStepDeg=3.0f,
                                   float ultraStepDeg=0.5f);
    
    // vec4 versions (w component is ignored, only xyz used)
    static OBB fromPointsMinVolume(const glm::vec4 *points,size_t count,
                                   float coarseStepDeg=15.0f,
                                   float fineStepDeg=3.0f,
                                   float ultraStepDeg=0.5f);
    static OBB fromPointsMinVolume(const std::vector<glm::vec4> &points,
                                   float coarseStepDeg=15.0f,
                                   float fineStepDeg=3.0f,
                                   float ultraStepDeg=0.5f);
};
