#include "math/BoundingSphere.h"

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
