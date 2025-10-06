#include "math/BoundingVolumes.h"

// Helper: sphere from AABB (center = mid, radius = half diagonal)
BoundingSphere SphereFromAABB(const AABB &a)
{
    BoundingSphere s; s.reset();
    if(!a.empty())
    {
        s.center=(a.min+a.max)*0.5f;
        s.radius=glm::length(a.max-s.center);
    }
    return s;
}

// Internal template implementation for SphereFromPoints
namespace
{
    template<typename VecType>
    BoundingSphere SphereFromPointsImpl(const std::vector<VecType> &pts)
    {
        BoundingSphere s; s.reset();
        if(pts.empty()) return s;
        
        glm::vec3 c(0.0f);
        for(const auto &p:pts) 
        {
            c.x += p.x;
            c.y += p.y;
            c.z += p.z;
        }
        c /= static_cast<float>(pts.size());
        
        float r=0.0f;
        for(const auto &p:pts)
        {
            glm::vec3 p3(p.x, p.y, p.z);
            r = std::max(r, glm::length(p3 - c));
        }
        
        s.center = c;
        s.radius = r;
        return s;
    }
}

// Helper: sphere from points (center = average, radius = max distance to center) using float input
BoundingSphere SphereFromPoints(const std::vector<glm::vec3> &pts)
{
    return SphereFromPointsImpl(pts);
}

BoundingSphere SphereFromPoints(const std::vector<glm::vec4> &pts)
{
    return SphereFromPointsImpl(pts);
}

void Write(std::ostream &ofs,const BoundingVolumes &volumes)
{
    {
        float fmin[3];
        float fmax[3];
        fmin[0]=volumes.aabb.min.x;
        fmin[1]=volumes.aabb.min.y;
        fmin[2]=volumes.aabb.min.z;
        fmax[0]=volumes.aabb.max.x;
        fmax[1]=volumes.aabb.max.y;
        fmax[2]=volumes.aabb.max.z;
        ofs.write(reinterpret_cast<const char *>(fmin),sizeof(fmin));
        ofs.write(reinterpret_cast<const char *>(fmax),sizeof(fmax));
    }

    {
        float center[3];
        float axisX[3];
        float axisY[3];
        float axisZ[3];
        float halfSize[3];

        center[0]=volumes.obb.center.x;
        center[1]=volumes.obb.center.y;
        center[2]=volumes.obb.center.z;

        axisX[0]=volumes.obb.axisX.x;
        axisX[1]=volumes.obb.axisX.y;
        axisX[2]=volumes.obb.axisX.z;

        axisY[0]=volumes.obb.axisY.x;
        axisY[1]=volumes.obb.axisY.y;
        axisY[2]=volumes.obb.axisY.z;

        axisZ[0]=volumes.obb.axisZ.x;
        axisZ[1]=volumes.obb.axisZ.y;
        axisZ[2]=volumes.obb.axisZ.z;

        halfSize[0]=volumes.obb.halfSize.x;
        halfSize[1]=volumes.obb.halfSize.y;
        halfSize[2]=volumes.obb.halfSize.z;

        ofs.write(reinterpret_cast<const char *>(center),sizeof(center));
        ofs.write(reinterpret_cast<const char *>(axisX),sizeof(axisX));
        ofs.write(reinterpret_cast<const char *>(axisY),sizeof(axisY));
        ofs.write(reinterpret_cast<const char *>(axisZ),sizeof(axisZ));
        ofs.write(reinterpret_cast<const char *>(halfSize),sizeof(halfSize));
    }

    {
        float center[3];
        float radius;
        center[0]=volumes.sphere.center.x;
        center[1]=volumes.sphere.center.y;
        center[2]=volumes.sphere.center.z;
        radius=volumes.sphere.radius;
        ofs.write(reinterpret_cast<const char *>(center),sizeof(center));
        ofs.write(reinterpret_cast<const char *>(&radius),sizeof(radius));
    }
}
