#include "math/OBB.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <immintrin.h>
#include <chrono>
#include <cstdio>
#ifdef _OPENMP
#include <omp.h>
#endif

void OBB::reset()
{
    center=glm::vec3(0.0f);
    axisX=glm::vec3(1.0f,0.0f,0.0f);
    axisY=glm::vec3(0.0f,1.0f,0.0f);
    axisZ=glm::vec3(0.0f,0.0f,1.0f);
    halfSize=glm::vec3(0.0f);
}

bool OBB::empty() const
{
    return (halfSize.x<=0.0f)||(halfSize.y<=0.0f)||(halfSize.z<=0.0f);
}

// Build an OBB from an AABB (axes aligned with world axes)
OBB OBB::fromAABB(const AABB &aabb)
{
    OBB obb;
    if(aabb.empty()) return obb;
    // Convert float AABB to float OBB
    obb.center=(aabb.min+aabb.max)*0.5f;
    obb.axisX=glm::vec3(1.0f,0.0f,0.0f);
    obb.axisY=glm::vec3(0.0f,1.0f,0.0f);
    obb.axisZ=glm::vec3(0.0f,0.0f,1.0f);
    obb.halfSize=(aabb.max-aabb.min)*0.5f;
    return obb;
}

// Construct an OBB from a transform matrix and half sizes in the local box space.
// The transform's linear part provides orientation and scale; translation sets the center.
OBB OBB::fromTransform(const glm::mat4 &m,const glm::vec3 &localHalfSize)
{
    OBB obb;
    const glm::mat3 L(m);
    const glm::vec3 col0=glm::vec3(L[0]);
    const glm::vec3 col1=glm::vec3(L[1]);
    const glm::vec3 col2=glm::vec3(L[2]);

    const float len0=glm::length(col0);
    const float len1=glm::length(col1);
    const float len2=glm::length(col2);

    obb.center=glm::vec3(m[3]);
    obb.axisX=(len0>0.0f)?(col0/len0):glm::vec3(1,0,0);
    obb.axisY=(len1>0.0f)?(col1/len1):glm::vec3(0,1,0);
    obb.axisZ=(len2>0.0f)?(col2/len2):glm::vec3(0,0,1);
    obb.halfSize=glm::vec3(localHalfSize.x*len0,localHalfSize.y*len1,localHalfSize.z*len2);
    return obb;
}

// Apply an affine transform to the OBB. For general linear transforms, axes are re-normalized
// and half sizes are scaled by the stretch along each transformed axis.
OBB OBB::transformed(const glm::mat4 &m) const
{
    if(empty()) return *this;
    OBB out;
    out.center=glm::vec3(m*glm::vec4(center,1.0f));
    const glm::mat3 L(m);

    const glm::vec3 v0=L*axisX;
    const glm::vec3 v1=L*axisY;
    const glm::vec3 v2=L*axisZ;

    const float l0=glm::length(v0);
    const float l1=glm::length(v1);
    const float l2=glm::length(v2);

    out.axisX=(l0>0.0f)?(v0/l0):axisX;
    out.axisY=(l1>0.0f)?(v1/l1):axisY;
    out.axisZ=(l2>0.0f)?(v2/l2):axisZ;
    out.halfSize=glm::vec3(halfSize.x*l0,halfSize.y*l1,halfSize.z*l2);
    return out;
}

// Convert this OBB to a tight AABB in the same space.
// Uses the well-known formula: extents = |R| * halfSize, where R has columns axisX, axisY, axisZ.
AABB OBB::toAABB() const
{
    AABB aabb;
    if(empty()) { aabb.reset(); return aabb; }
    const glm::vec3 ax=glm::abs(axisX);
    const glm::vec3 ay=glm::abs(axisY);
    const glm::vec3 az=glm::abs(axisZ);
    const glm::vec3 e=ax*halfSize.x+ay*halfSize.y+az*halfSize.z;
    aabb.min=center-e;
    aabb.max=center+e;
    return aabb;
}

// Fill an array with the 8 corners of the box, if needed by clients.
void OBB::corners(glm::vec3 out[8]) const
{
    const glm::vec3 ex=axisX*halfSize.x;
    const glm::vec3 ey=axisY*halfSize.y;
    const glm::vec3 ez=axisZ*halfSize.z;
    out[0]=center-ex-ey-ez;
    out[1]=center+ex-ey-ez;
    out[2]=center-ex+ey-ez;
    out[3]=center+ex+ey-ez;
    out[4]=center-ex-ey+ez;
    out[5]=center+ex-ey+ez;
    out[6]=center-ex+ey+ez;
    out[7]=center+ex+ey+ez;
}

// Extremely thorough minimum-volume OBB by global orientation search (slow, memory heavy OK).
// Internal template implementation that works with both vec3 and vec4
namespace
{
    template<typename VecType>
    OBB fromPointsMinVolumeImpl(const VecType *points,size_t count,float coarseStepDeg,float fineStepDeg,float ultraStepDeg)
    {
        auto t0=std::chrono::high_resolution_clock::now();
        OBB best; if(points==nullptr||count==0) return best;
        if(count==1) {
            best.center=glm::vec3(points[0].x,points[0].y,points[0].z);
            return best;
        }

        // SoA 预处理，便于 AVX2 连续加载（glm::vec3/vec4 在当前编译设置下为 16 字节对齐，存在填充，不利直接批量 load）
        std::vector<float> xs(count),ys(count),zs(count);
    #ifdef _OPENMP
    #pragma omp parallel for if(count > 1024)
    #endif
        for(int i=0; i<static_cast<int>(count); ++i)
        {
            xs[i]=points[i].x;
            ys[i]=points[i].y;
            zs[i]=points[i].z;
        }

        auto makeR=[](float yawDeg,float pitchDeg,float rollDeg)
            {
                const float k=3.14159265358979323846f/180.0f;
                float yaw=yawDeg*k,pitch=pitchDeg*k,roll=rollDeg*k;
                glm::quat qz=glm::angleAxis(yaw,glm::vec3(0,0,1));
                glm::quat qy=glm::angleAxis(pitch,glm::vec3(0,1,0));
                glm::quat qx=glm::angleAxis(roll,glm::vec3(1,0,0));
                glm::quat q=qz*qy*qx;
                return glm::mat3_cast(q);
            };

        // 向量化 + 标量回退的投影评估
        auto evalOrientation=[&](const glm::mat3 &R,OBB &out)
            {
                const glm::vec3 U=glm::vec3(R[0]);
                const glm::vec3 V=glm::vec3(R[1]);
                const glm::vec3 W=glm::vec3(R[2]);
                float minU=std::numeric_limits<float>::infinity(),maxU=-minU;
                float minV=std::numeric_limits<float>::infinity(),maxV=-minV;
                float minW=std::numeric_limits<float>::infinity(),maxW=-minW;

            #if defined(__AVX2__)
                const size_t N=count-(count%8);
                __m256 Ux=_mm256_set1_ps(U.x); __m256 Uy=_mm256_set1_ps(U.y); __m256 Uz=_mm256_set1_ps(U.z);
                __m256 Vx=_mm256_set1_ps(V.x); __m256 Vy=_mm256_set1_ps(V.y); __m256 Vz=_mm256_set1_ps(V.z);
                __m256 Wx=_mm256_set1_ps(W.x); __m256 Wy=_mm256_set1_ps(W.y); __m256 Wz=_mm256_set1_ps(W.z);

                __m256 minU8=_mm256_set1_ps(std::numeric_limits<float>::infinity());
                __m256 maxU8=_mm256_set1_ps(-std::numeric_limits<float>::infinity());
                __m256 minV8=minU8,maxV8=maxU8;
                __m256 minW8=minU8,maxW8=maxU8;

                for(size_t i=0;i<N;i+=8)
                {
                    __m256 X=_mm256_loadu_ps(xs.data()+i);
                    __m256 Y=_mm256_loadu_ps(ys.data()+i);
                    __m256 Z=_mm256_loadu_ps(zs.data()+i);
                    __m256 pu=_mm256_fmadd_ps(Uz,Z,_mm256_fmadd_ps(Uy,Y,_mm256_mul_ps(Ux,X))); // X*Ux + Y*Uy + Z*Uz
                    __m256 pv=_mm256_fmadd_ps(Vz,Z,_mm256_fmadd_ps(Vy,Y,_mm256_mul_ps(Vx,X)));
                    __m256 pw=_mm256_fmadd_ps(Wz,Z,_mm256_fmadd_ps(Wy,Y,_mm256_mul_ps(Wx,X)));
                    minU8=_mm256_min_ps(minU8,pu); maxU8=_mm256_max_ps(maxU8,pu);
                    minV8=_mm256_min_ps(minV8,pv); maxV8=_mm256_max_ps(maxV8,pv);
                    minW8=_mm256_min_ps(minW8,pw); maxW8=_mm256_max_ps(maxW8,pw);
                }
                alignas(32) float buMin[8],buMax[8],bvMin[8],bvMax[8],bwMin[8],bwMax[8];
                _mm256_store_ps(buMin,minU8); _mm256_store_ps(buMax,maxU8);
                _mm256_store_ps(bvMin,minV8); _mm256_store_ps(bvMax,maxV8);
                _mm256_store_ps(bwMin,minW8); _mm256_store_ps(bwMax,maxW8);
                for(int k=0;k<8;++k)
                {
                    if(buMin[k]<minU) minU=buMin[k]; if(buMax[k]>maxU) maxU=buMax[k];
                    if(bvMin[k]<minV) minV=bvMin[k]; if(bvMax[k]>maxV) maxV=bvMax[k];
                    if(bwMin[k]<minW) minW=bwMin[k]; if(bwMax[k]>maxW) maxW=bwMax[k];
                }
                // 处理余数
                for(size_t i=N;i<count;++i)
                {
                    float x=xs[i],y=ys[i],z=zs[i];
                    float pu=x*U.x+y*U.y+z*U.z; if(pu<minU) minU=pu; if(pu>maxU) maxU=pu;
                    float pv=x*V.x+y*V.y+z*V.z; if(pv<minV) minV=pv; if(pv>maxV) maxV=pv;
                    float pw=x*W.x+y*W.y+z*W.z; if(pw<minW) minW=pw; if(pw>maxW) maxW=pw;
                }
            #else
                for(size_t i=0;i<count;++i)
                {
                    float x=xs[i],y=ys[i],z=zs[i];
                    float pu=x*U.x+y*U.y+z*U.z; if(pu<minU) minU=pu; if(pu>maxU) maxU=pu;
                    float pv=x*V.x+y*V.y+z*V.z; if(pv<minV) minV=pv; if(pv>maxV) maxV=pv;
                    float pw=x*W.x+y*W.y+z*W.z; if(pw<minW) minW=pw; if(pw>maxW) maxW=pw;
                }
            #endif
                float sx=maxU-minU,sy=maxV-minV,sz=maxW-minW;
                float volume=sx*sy*sz;
                out.center=U*(0.5f*(minU+maxU))+V*(0.5f*(minV+maxV))+W*(0.5f*(minW+maxW));
                out.axisX=U; out.axisY=V; out.axisZ=W;
                out.halfSize=glm::vec3(0.5f*sx,0.5f*sy,0.5f*sz);
                return volume;
            };

        int yawSteps=static_cast<int>(360.0f/coarseStepDeg)+1;
        int pitchSteps=static_cast<int>(180.0f/coarseStepDeg)+1;
        int rollSteps=static_cast<int>(360.0f/coarseStepDeg)+1;
        float bestYaw=0,bestPitch=0,bestRoll=0; float bestVol=std::numeric_limits<float>::infinity();
        OBB bestTmp,tmp;

        // 粗搜索并行
    #ifdef _OPENMP
    #pragma omp parallel
        {
            OBB localBestBox; OBB localTmp;
            float localBestVol=std::numeric_limits<float>::infinity();
            float localYaw=0,localPitch=0,localRoll=0;
            int total=yawSteps*pitchSteps*rollSteps;
        #pragma omp for schedule(static)
            for(int idx=0; idx<total; ++idx)
            {
                int block=pitchSteps*rollSteps;
                int iyaw=idx/block;
                int rem=idx-iyaw*block;
                int ipitch=rem/rollSteps;
                int iroll=rem-ipitch*rollSteps;
                float yaw=-180.0f+iyaw*coarseStepDeg;
                float pitch=-90.0f+ipitch*coarseStepDeg;
                float roll=-180.0f+iroll*coarseStepDeg;
                glm::mat3 R=makeR(yaw,pitch,roll);
                float vol=evalOrientation(R,localTmp);
                if(vol<localBestVol) { localBestVol=vol; localBestBox=localTmp; localYaw=yaw; localPitch=pitch; localRoll=roll; }
            }
        #pragma omp critical
            {
                if(localBestVol<bestVol) { bestVol=localBestVol; bestTmp=localBestBox; bestYaw=localYaw; bestPitch=localPitch; bestRoll=localRoll; }
            }
        }
    #else
        for(int iyaw=0; iyaw<yawSteps; ++iyaw)
        {
            float yaw=-180.0f+iyaw*coarseStepDeg;
            for(int ipitch=0; ipitch<pitchSteps; ++ipitch)
            {
                float pitch=-90.0f+ipitch*coarseStepDeg;
                for(int iroll=0; iroll<rollSteps; ++iroll)
                {
                    float roll=-180.0f+iroll*coarseStepDeg;
                    glm::mat3 R=makeR(yaw,pitch,roll);
                    float vol=evalOrientation(R,tmp);
                    if(vol<bestVol) { bestVol=vol; bestTmp=tmp; bestYaw=yaw; bestPitch=pitch; bestRoll=roll; }
                }
            }
        }
    #endif
        best=bestTmp;

        auto refine=[&](float rangeDeg,float stepDeg)
            {
                float y0=bestYaw,p0=bestPitch,r0=bestRoll;
                int steps=static_cast<int>((2*rangeDeg)/stepDeg)+1; // 每个轴离散点数
            #ifdef _OPENMP
            #pragma omp parallel
                {
                    OBB localBest=best; float localBestVol=bestVol;
                    float localYaw=y0,localPitch=p0,localRoll=r0;
                    int total=steps*steps*steps;
                #pragma omp for schedule(static)
                for(int idx=0; idx<total; ++idx)
                {
                    int plane=steps*steps;
                    int iy=idx/plane; int rem=idx-iy*plane;
                    int ip=rem/steps; int ir=rem-ip*steps;
                    float dy=-rangeDeg+iy*stepDeg;
                    float dp=-rangeDeg+ip*stepDeg;
                    float dr=-rangeDeg+ir*stepDeg;
                    glm::mat3 R=makeR(y0+dy,p0+dp,r0+dr);
                    OBB tmpLocal; float vol=evalOrientation(R,tmpLocal);
                    if(vol<localBestVol) { localBestVol=vol; localBest=tmpLocal; localYaw=y0+dy; localPitch=p0+dp; localRoll=r0+dr; }
                }
            #pragma omp critical
                {
                    if(localBestVol<bestVol) { bestVol=localBestVol; best=localBest; bestYaw=localYaw; bestPitch=localPitch; bestRoll=localRoll; }
                }
            }
            #else
                for(float dy=-rangeDeg; dy<=rangeDeg; dy+=stepDeg)
                {
                    for(float dp=-rangeDeg; dp<=rangeDeg; dp+=stepDeg)
                    {
                        for(float dr=-rangeDeg; dr<=rangeDeg; dr+=stepDeg)
                        {
                            glm::mat3 R=makeR(y0+dy,p0+dp,r0+dr);
                            float vol=evalOrientation(R,tmp);
                            if(vol<bestVol) { bestVol=vol; best=tmp; bestYaw=y0+dy; bestPitch=p0+dp; bestRoll=r0+dr; }
                        }
                    }
                }
            #endif
            };

        refine(15.0f,fineStepDeg);
        refine(3.0f,ultraStepDeg);

        auto t1=std::chrono::high_resolution_clock::now();
        float ms=std::chrono::duration<float,std::milli>(t1-t0).count();
        std::printf("OBB::fromPointsMinVolume points: %zu time: %.3f ms (OpenMP %s, AVX2 %s)\n",count,ms,
                #ifdef _OPENMP
                    "ON",
                #else
                    "OFF",
                #endif
                #ifdef __AVX2__
                    "ON"
                #else
                    "OFF"
                #endif
        );
        return best;
    }
} // anonymous namespace

OBB OBB::fromPointsMinVolume(const glm::vec3 *points,size_t count,float coarseStepDeg,float fineStepDeg,float ultraStepDeg)
{
    return fromPointsMinVolumeImpl(points,count,coarseStepDeg,fineStepDeg,ultraStepDeg);
}

OBB OBB::fromPointsMinVolume(const std::vector<glm::vec3> &points,float coarseStepDeg,float fineStepDeg,float ultraStepDeg)
{
    return fromPointsMinVolume(points.data(),points.size(),coarseStepDeg,fineStepDeg,ultraStepDeg);
}

OBB OBB::fromPointsMinVolume(const glm::vec4 *points,size_t count,float coarseStepDeg,float fineStepDeg,float ultraStepDeg)
{
    return fromPointsMinVolumeImpl(points,count,coarseStepDeg,fineStepDeg,ultraStepDeg);
}

OBB OBB::fromPointsMinVolume(const std::vector<glm::vec4> &points,float coarseStepDeg,float fineStepDeg,float ultraStepDeg)
{
    return fromPointsMinVolume(points.data(),points.size(),coarseStepDeg,fineStepDeg,ultraStepDeg);
}
