#include"OBB.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <immintrin.h> // for AVX2 intrinsics

void OBB::reset() {
    center = glm::dvec3(0.0);
    axisX = glm::dvec3(1.0,0.0,0.0);
    axisY = glm::dvec3(0.0,1.0,0.0);
    axisZ = glm::dvec3(0.0,0.0,1.0);
    halfSize = glm::dvec3(0.0);
}

bool OBB::empty() const {
    return (halfSize.x <= 0.0) || (halfSize.y <= 0.0) || (halfSize.z <= 0.0);
}

// Build an OBB from an AABB (axes aligned with world axes)
OBB OBB::fromAABB(const AABB &aabb) {
    OBB obb;
    if(aabb.empty()) return obb;
    obb.center = (aabb.min + aabb.max) * 0.5;
    obb.axisX = glm::dvec3(1.0,0.0,0.0);
    obb.axisY = glm::dvec3(0.0,1.0,0.0);
    obb.axisZ = glm::dvec3(0.0,0.0,1.0);
    obb.halfSize = (aabb.max - aabb.min) * 0.5;
    return obb;
}

// Construct an OBB from a transform matrix and half sizes in the local box space.
// The transform's linear part provides orientation and scale; translation sets the center.
OBB OBB::fromTransform(const glm::dmat4 &m,const glm::dvec3 &localHalfSize) {
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
    obb.halfSize = glm::dvec3(localHalfSize.x * len0,localHalfSize.y * len1,localHalfSize.z * len2);
    return obb;
}

// Apply an affine transform to the OBB. For general linear transforms, axes are re-normalized
// and half sizes are scaled by the stretch along each transformed axis.
OBB OBB::transformed(const glm::dmat4 &m) const {
    if(empty()) return *this;
    OBB out;
    out.center = glm::dvec3(m * glm::dvec4(center,1.0));
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
    out.halfSize = glm::dvec3(halfSize.x * l0,halfSize.y * l1,halfSize.z * l2);
    return out;
}

// Convert this OBB to a tight AABB in the same space.
// Uses the well-known formula: extents = |R| * halfSize, where R has columns axisX, axisY, axisZ.
AABB OBB::toAABB() const {
    AABB aabb;
    if(empty()) {
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
void OBB::corners(glm::dvec3 out[8]) const {
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
OBB OBB::fromPointsMinVolume(const glm::dvec3 *points,size_t count,double coarseStepDeg,double fineStepDeg,double ultraStepDeg) {
    OBB best; // empty initially
    if(points == nullptr || count == 0) return best;
    if(count == 1) { best.center = points[0]; return best; }

    auto makeR = [](double yawDeg,double pitchDeg,double rollDeg) {
        const double k = 3.14159265358979323846264338327950288 / 180.0;
        const double yaw = yawDeg * k;
        const double pitch = pitchDeg * k;
        const double roll = rollDeg * k;
        const glm::dquat qz = glm::angleAxis(yaw,glm::dvec3(0,0,1));
        const glm::dquat qy = glm::angleAxis(pitch,glm::dvec3(0,1,0));
        const glm::dquat qx = glm::angleAxis(roll,glm::dvec3(1,0,0));
        const glm::dquat q = qz * qy * qx;
        return glm::mat3_cast(q);
        };

    auto evalOrientation = [&](const glm::dmat3 &R,OBB &out) {
        // R is built from a unit quaternion => columns are already orthonormal.
        const glm::dvec3 U = glm::dvec3(R[0]);
        const glm::dvec3 V = glm::dvec3(R[1]);
        const glm::dvec3 W = glm::dvec3(R[2]);

        double minU = std::numeric_limits<double>::infinity();
        double maxU = -std::numeric_limits<double>::infinity();
        double minV = std::numeric_limits<double>::infinity();
        double maxV = -std::numeric_limits<double>::infinity();
        double minW = std::numeric_limits<double>::infinity();
        double maxW = -std::numeric_limits<double>::infinity();

#if defined(__AVX__)
        // AVX vectorized dot products and min/max
        const size_t N = count;
        size_t i = 0;
        __m256d minU4 = _mm256_set1_pd(minU),maxU4 = _mm256_set1_pd(maxU);
        __m256d minV4 = _mm256_set1_pd(minV),maxV4 = _mm256_set1_pd(maxV);
        __m256d minW4 = _mm256_set1_pd(minW),maxW4 = _mm256_set1_pd(maxW);
        const __m256d Ux = _mm256_set1_pd(U.x),Uy = _mm256_set1_pd(U.y),Uz = _mm256_set1_pd(U.z);
        const __m256d Vx = _mm256_set1_pd(V.x),Vy = _mm256_set1_pd(V.y),Vz = _mm256_set1_pd(V.z);
        const __m256d Wx = _mm256_set1_pd(W.x),Wy = _mm256_set1_pd(W.y),Wz = _mm256_set1_pd(W.z);
        for(; i + 3 < N; i += 4) {
            __m256d px = _mm256_set_pd(points[i + 3].x,points[i + 2].x,points[i + 1].x,points[i].x);
            __m256d py = _mm256_set_pd(points[i + 3].y,points[i + 2].y,points[i + 1].y,points[i].y);
            __m256d pz = _mm256_set_pd(points[i + 3].z,points[i + 2].z,points[i + 1].z,points[i].z);
            __m256d pu = _mm256_add_pd(_mm256_add_pd(_mm256_mul_pd(px,Ux),_mm256_mul_pd(py,Uy)),_mm256_mul_pd(pz,Uz));
            __m256d pv = _mm256_add_pd(_mm256_add_pd(_mm256_mul_pd(px,Vx),_mm256_mul_pd(py,Vy)),_mm256_mul_pd(pz,Vz));
            __m256d pw = _mm256_add_pd(_mm256_add_pd(_mm256_mul_pd(px,Wx),_mm256_mul_pd(py,Wy)),_mm256_mul_pd(pz,Wz));
            minU4 = _mm256_min_pd(minU4,pu); maxU4 = _mm256_max_pd(maxU4,pu);
            minV4 = _mm256_min_pd(minV4,pv); maxV4 = _mm256_max_pd(maxV4,pv);
            minW4 = _mm256_min_pd(minW4,pw); maxW4 = _mm256_max_pd(maxW4,pw);
        }
        // Reduce vector min/max to scalars
        double minUarr[4],maxUarr[4],minVarr[4],maxVarr[4],minWarr[4],maxWarr[4];
        _mm256_storeu_pd(minUarr,minU4); _mm256_storeu_pd(maxUarr,maxU4);
        _mm256_storeu_pd(minVarr,minV4); _mm256_storeu_pd(maxVarr,maxV4);
        _mm256_storeu_pd(minWarr,minW4); _mm256_storeu_pd(maxWarr,maxW4);
        for(int j = 0; j < 4; ++j) {
            if(minUarr[j] < minU) minU = minUarr[j]; if(maxUarr[j] > maxU) maxU = maxUarr[j];
            if(minVarr[j] < minV) minV = minVarr[j]; if(maxVarr[j] > maxV) maxV = maxVarr[j];
            if(minWarr[j] < minW) minW = minWarr[j]; if(maxWarr[j] > maxW) maxW = maxWarr[j];
        }
        // Tail
        for(; i < N; ++i) {
            const glm::dvec3 &p = points[i];
            const double pu = glm::dot(p,U);
            const double pv = glm::dot(p,V);
            const double pw = glm::dot(p,W);
            if(pu < minU) minU = pu; if(pu > maxU) maxU = pu;
            if(pv < minV) minV = pv; if(pv > maxV) maxV = pv;
            if(pw < minW) minW = pw; if(pw > maxW) maxW = pw;
        }
#else
        // 标量实现
        for(size_t i = 0; i < count; ++i) {
            const glm::dvec3 &p = points[i];
            const double pu = glm::dot(p,U);
            const double pv = glm::dot(p,V);
            const double pw = glm::dot(p,W);
            if(pu < minU) minU = pu; if(pu > maxU) maxU = pu;
            if(pv < minV) minV = pv; if(pv > maxV) maxV = pv;
            if(pw < minW) minW = pw; if(pw > maxW) maxW = pw;
        }
#endif // __AVX__
        const double sx = (maxU - minU);
        const double sy = (maxV - minV);
        const double sz = (maxW - minW);
        const double volume = sx * sy * sz;
        out.center = U * (0.5 * (minU + maxU)) + V * (0.5 * (minV + maxV)) + W * (0.5 * (minW + maxW));
        out.axisX = U; out.axisY = V; out.axisZ = W;
        out.halfSize = glm::dvec3(0.5 * sx,0.5 * sy,0.5 * sz);
        return volume;
        };

    // Phase 1: coarse global search
    // 预计算所有可能的sin/cos，避免循环内重复计算
    const int yawSteps = static_cast<int>(360.0 / coarseStepDeg) + 1;
    const int pitchSteps = static_cast<int>(180.0 / coarseStepDeg) + 1;
    const int rollSteps = static_cast<int>(360.0 / coarseStepDeg) + 1;
    std::vector<double> yawVals(yawSteps), yawSin(yawSteps), yawCos(yawSteps);
    std::vector<double> pitchVals(pitchSteps), pitchSin(pitchSteps), pitchCos(pitchSteps);
    std::vector<double> rollVals(rollSteps), rollSin(rollSteps), rollCos(rollSteps);
    const double k = 3.14159265358979323846264338327950288 / 180.0;
    for(int i=0; i<yawSteps; ++i) {
        yawVals[i] = -180.0 + i * coarseStepDeg;
        yawSin[i] = sin(yawVals[i] * k);
        yawCos[i] = cos(yawVals[i] * k);
    }
    for(int i=0; i<pitchSteps; ++i) {
        pitchVals[i] = -90.0 + i * coarseStepDeg;
        pitchSin[i] = sin(pitchVals[i] * k);
        pitchCos[i] = cos(pitchVals[i] * k);
    }
    for(int i=0; i<rollSteps; ++i) {
        rollVals[i] = -180.0 + i * coarseStepDeg;
        rollSin[i] = sin(rollVals[i] * k);
        rollCos[i] = cos(rollVals[i] * k);
    }
    double bestYaw = 0,bestPitch = 0,bestRoll = 0;
    double bestVol = std::numeric_limits<double>::infinity();
    OBB tmp;
    for(int iyaw=0; iyaw<yawSteps; ++iyaw) {
        for(int ipitch=0; ipitch<pitchSteps; ++ipitch) {
            for(int iroll=0; iroll<rollSteps; ++iroll) {
                // 用预计算的sin/cos构造四元数
                double yaw = yawVals[iyaw], pitch = pitchVals[ipitch], roll = rollVals[iroll];
                double sy = yawSin[iyaw], cy = yawCos[iyaw];
                double sp = pitchSin[ipitch], cp = pitchCos[ipitch];
                double sr = rollSin[iroll], cr = rollCos[iroll];
                // ZYX欧拉角转四元数
                glm::dquat qz = glm::dquat(cy, 0, 0, sy); // 只近似用于sin/cos复用
                glm::dquat qy = glm::dquat(cp, 0, sp, 0);
                glm::dquat qx = glm::dquat(cr, sr, 0, 0);
                glm::dquat q = qz * qy * qx;
                glm::dmat3 R = glm::mat3_cast(q);
                double vol = evalOrientation(R,tmp);
                if(vol < bestVol) {
                    bestVol = vol; best = tmp; bestYaw = yaw; bestPitch = pitch; bestRoll = roll;
                }
            }
        }
    }

    // Phase 2: refine around best
    auto refine = [&](double rangeDeg,double stepDeg){
        const double y0 = bestYaw,p0 = bestPitch,r0 = bestRoll;
        for(double dy = -rangeDeg; dy <= rangeDeg; dy += stepDeg) {
            for(double dp = -rangeDeg; dp <= rangeDeg; dp += stepDeg) {
                for(double dr = -rangeDeg; dr <= rangeDeg; dr += stepDeg) {
                    const glm::dmat3 R = makeR(y0 + dy,p0 + dp,r0 + dr);
                    const double vol = evalOrientation(R,tmp);
                    if(vol < bestVol) {
                        bestVol = vol; best = tmp; bestYaw = y0 + dy; bestPitch = p0 + dp; bestRoll = r0 + dr;
                    }
                }
            }
        }
        };

    refine(15.0,fineStepDeg);
    refine(3.0,ultraStepDeg);

    return best;
}

OBB OBB::fromPointsMinVolume(const std::vector<glm::dvec3> &points,double coarseStepDeg,double fineStepDeg,double ultraStepDeg)
{
    return fromPointsMinVolume(points.data(),points.size(),coarseStepDeg,fineStepDeg,ultraStepDeg);
}
