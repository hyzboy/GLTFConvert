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

    // Compute an OBB that tightly bounds a set of points using PCA for orientation,
    // then exact min/max extents along those axes. This is fast and robust.
    static OBB fromPoints(const glm::dvec3* points, size_t count) {
        OBB obb;
        if (points == nullptr || count == 0) {
            return obb; // empty
        }
        if (count == 1) {
            obb.center = points[0];
            obb.axisX = glm::dvec3(1,0,0);
            obb.axisY = glm::dvec3(0,1,0);
            obb.axisZ = glm::dvec3(0,0,1);
            obb.halfSize = glm::dvec3(0);
            return obb;
        }

        // Mean
        glm::dvec3 mean(0.0);
        for (size_t i = 0; i < count; ++i) mean += points[i];
        mean /= static_cast<double>(count);

        // Covariance matrix (symmetric)
        double cxx = 0, cxy = 0, cxz = 0;
        double cyy = 0, cyz = 0, czz = 0;
        for (size_t i = 0; i < count; ++i) {
            const glm::dvec3 d = points[i] - mean;
            cxx += d.x * d.x;
            cxy += d.x * d.y;
            cxz += d.x * d.z;
            cyy += d.y * d.y;
            cyz += d.y * d.z;
            czz += d.z * d.z;
        }
        const double denom = static_cast<double>(count);
        cxx /= denom; cxy /= denom; cxz /= denom;
        cyy /= denom; cyz /= denom; czz /= denom;

        // Jacobi eigen-decomposition for symmetric 3x3
        auto jacobiEigenSymmetric3x3 = [](double a[3][3], double V[3][3]) {
            // Initialize V to identity
            V[0][0] = 1; V[0][1] = 0; V[0][2] = 0;
            V[1][0] = 0; V[1][1] = 1; V[1][2] = 0;
            V[2][0] = 0; V[2][1] = 0; V[2][2] = 1;

            const int maxIters = 50;
            for (int iter = 0; iter < maxIters; ++iter) {
                // Find largest off-diagonal element
                int p = 0, q = 1;
                double maxOff = std::fabs(a[0][1]);
                if (std::fabs(a[0][2]) > maxOff) { p = 0; q = 2; maxOff = std::fabs(a[0][2]); }
                if (std::fabs(a[1][2]) > maxOff) { p = 1; q = 2; maxOff = std::fabs(a[1][2]); }
                if (maxOff < 1e-15) break;

                double app = a[p][p];
                double aqq = a[q][q];
                double apq = a[p][q];

                double theta = (aqq - app) / (2.0 * apq);
                double t = 1.0 / (std::fabs(theta) + std::sqrt(theta * theta + 1.0));
                if (theta < 0.0) t = -t;
                double c = 1.0 / std::sqrt(1.0 + t * t);
                double s = t * c;

                // Rotate rows/cols p and q in 'a'
                for (int k = 0; k < 3; ++k) {
                    if (k != p && k != q) {
                        double aik = a[p][k];
                        double aqk = a[q][k];
                        double apk_new = c * aik - s * aqk;
                        double aqk_new = s * aik + c * aqk;
                        a[p][k] = a[k][p] = apk_new;
                        a[q][k] = a[k][q] = aqk_new;
                    }
                }
                double app_new = c * c * app - 2.0 * s * c * apq + s * s * aqq;
                double aqq_new = s * s * app + 2.0 * s * c * apq + c * c * aqq;
                a[p][p] = app_new;
                a[q][q] = aqq_new;
                a[p][q] = a[q][p] = 0.0;

                // Accumulate rotation into V (columns are eigenvectors)
                for (int k = 0; k < 3; ++k) {
                    double vip = V[k][p];
                    double viq = V[k][q];
                    V[k][p] = c * vip - s * viq;
                    V[k][q] = s * vip + c * viq;
                }
            }
        };

        double a[3][3] = {
            { cxx, cxy, cxz },
            { cxy, cyy, cyz },
            { cxz, cyz, czz }
        };
        double Vrm[3][3];
        jacobiEigenSymmetric3x3(a, Vrm);

        // Extract eigenvalues and eigenvectors
        double evals[3] = { a[0][0], a[1][1], a[2][2] };
        glm::dvec3 evecs[3] = {
            glm::dvec3(Vrm[0][0], Vrm[1][0], Vrm[2][0]),
            glm::dvec3(Vrm[0][1], Vrm[1][1], Vrm[2][1]),
            glm::dvec3(Vrm[0][2], Vrm[1][2], Vrm[2][2])
        };

        // Sort by descending eigenvalue
        int idx[3] = {0,1,2};
        std::sort(idx, idx+3, [&](int i, int j){ return evals[i] > evals[j]; });
        glm::dvec3 U = glm::normalize(evecs[idx[0]]);
        glm::dvec3 V = glm::normalize(evecs[idx[1]]);
        glm::dvec3 W = glm::normalize(evecs[idx[2]]);

        // Ensure orthonormal and right-handed
        // Re-orthogonalize V, W against U if necessary (numerical safety)
        V = glm::normalize(V - U * glm::dot(U, V));
        W = glm::cross(U, V);
        if (glm::dot(W, W) < 1e-30) {
            // Degenerate; pick an arbitrary orthogonal basis
            V = glm::normalize(glm::abs(U.x) > 0.9 ? glm::dvec3(0,1,0) : glm::cross(U, glm::dvec3(1,0,0)));
            W = glm::normalize(glm::cross(U, V));
        }
        if (glm::dot(glm::cross(U, V), W) < 0.0) W = -W;

        // Project points on axes to get min/max
        double minU = std::numeric_limits<double>::infinity();
        double maxU = -std::numeric_limits<double>::infinity();
        double minV = std::numeric_limits<double>::infinity();
        double maxV = -std::numeric_limits<double>::infinity();
        double minW = std::numeric_limits<double>::infinity();
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

        const double midU = 0.5 * (minU + maxU);
        const double midV = 0.5 * (minV + maxV);
        const double midW = 0.5 * (minW + maxW);
        obb.center = U * midU + V * midV + W * midW;
        obb.axisX = U;
        obb.axisY = V;
        obb.axisZ = W;
        obb.halfSize = glm::dvec3(0.5 * (maxU - minU), 0.5 * (maxV - minV), 0.5 * (maxW - minW));

        return obb;
    }

    static OBB fromPoints(const std::vector<glm::dvec3>& points) {
        return fromPoints(points.data(), points.size());
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
