#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
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

    // Extremely thorough minimum-volume OBB by global orientation search (slow, memory heavy OK).
    // Performs coarse-to-fine search over yaw(Z), pitch(Y), roll(X) and evaluates exact extents.
    static OBB fromPointsMinVolume(const glm::dvec3* points, size_t count,
                                   double coarseStepDeg = 15.0,
                                   double fineStepDeg = 3.0,
                                   double ultraStepDeg = 0.5) {
        OBB best; // empty initially
        if (points == nullptr || count == 0) return best;
        if (count == 1) { best.center = points[0]; return best; }

        auto makeR = [](double yawDeg, double pitchDeg, double rollDeg) {
            const double k = 3.14159265358979323846264338327950288 / 180.0;
            const double yaw = yawDeg * k;
            const double pitch = pitchDeg * k;
            const double roll = rollDeg * k;
            const glm::dquat qz = glm::angleAxis(yaw,   glm::dvec3(0,0,1));
            const glm::dquat qy = glm::angleAxis(pitch, glm::dvec3(0,1,0));
            const glm::dquat qx = glm::angleAxis(roll,  glm::dvec3(1,0,0));
            const glm::dquat q = qz * qy * qx;
            return glm::mat3_cast(q);
        };

        auto evalOrientation = [&](const glm::dmat3& R, OBB& out) {
            const glm::dvec3 U = glm::normalize(glm::dvec3(R[0][0], R[1][0], R[2][0]));
            const glm::dvec3 V = glm::normalize(glm::dvec3(R[0][1], R[1][1], R[2][1]));
            glm::dvec3 W = glm::cross(U, V);
            if (glm::dot(W, W) < 1e-30) return std::numeric_limits<double>::infinity();
            W = glm::normalize(W);
            if (glm::dot(glm::cross(U, V), W) < 0.0) W = -W;

            double minU =  std::numeric_limits<double>::infinity();
            double maxU = -std::numeric_limits<double>::infinity();
            double minV =  std::numeric_limits<double>::infinity();
            double maxV = -std::numeric_limits<double>::infinity();
            double minW =  std::numeric_limits<double>::infinity();
            double maxW = -std::numeric_limits<double>::infinity();
            for (size_t i = 0; i < count; ++i) {
                const glm::dvec3& p = points[i];
                const double pu = glm::dot(p, U);
                const double pv = glm::dot(p, V);
                const double pw = glm::dot(p, W);
                if (pu < minU) minU = pu; if (pu > maxU) maxU = pu;
                if (pv < minV) minV = pv; if (pv > maxV) maxV = pv;
                if (pw < minW) minW = pw; if (pw > maxW) maxW = pw;
            }
            const double sx = (maxU - minU);
            const double sy = (maxV - minV);
            const double sz = (maxW - minW);
            const double volume = sx * sy * sz;
            out.center = U * (0.5 * (minU + maxU)) + V * (0.5 * (minV + maxV)) + W * (0.5 * (minW + maxW));
            out.axisX = U; out.axisY = V; out.axisZ = W;
            out.halfSize = glm::dvec3(0.5 * sx, 0.5 * sy, 0.5 * sz);
            return volume;
        };

        // Phase 1: coarse global search
        double bestYaw = 0, bestPitch = 0, bestRoll = 0;
        double bestVol = std::numeric_limits<double>::infinity();
        OBB tmp;
        for (double yaw = -180.0; yaw <= 180.0; yaw += coarseStepDeg) {
            for (double pitch = -90.0; pitch <= 90.0; pitch += coarseStepDeg) {
                for (double roll = -180.0; roll <= 180.0; roll += coarseStepDeg) {
                    const glm::dmat3 R = makeR(yaw, pitch, roll);
                    const double vol = evalOrientation(R, tmp);
                    if (vol < bestVol) {
                        bestVol = vol; best = tmp; bestYaw = yaw; bestPitch = pitch; bestRoll = roll;
                    }
                }
            }
        }

        // Phase 2: refine around best
        auto refine = [&](double rangeDeg, double stepDeg){
            const double y0 = bestYaw, p0 = bestPitch, r0 = bestRoll;
            for (double dy = -rangeDeg; dy <= rangeDeg; dy += stepDeg) {
                for (double dp = -rangeDeg; dp <= rangeDeg; dp += stepDeg) {
                    for (double dr = -rangeDeg; dr <= rangeDeg; dr += stepDeg) {
                        const glm::dmat3 R = makeR(y0+dy, p0+dp, r0+dr);
                        const double vol = evalOrientation(R, tmp);
                        if (vol < bestVol) {
                            bestVol = vol; best = tmp; bestYaw = y0+dy; bestPitch = p0+dp; bestRoll = r0+dr;
                        }
                    }
                }
            }
        };

        refine(15.0, fineStepDeg);
        refine(3.0, ultraStepDeg);

        return best;
    }

    static OBB fromPointsMinVolume(const std::vector<glm::dvec3>& points,
                                   double coarseStepDeg = 15.0,
                                   double fineStepDeg = 3.0,
                                   double ultraStepDeg = 0.5) {
        return fromPointsMinVolume(points.data(), points.size(), coarseStepDeg, fineStepDeg, ultraStepDeg);
    }
};
