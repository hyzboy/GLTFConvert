#include "pure/ModelCore.h"

namespace pure
{

    static bool Mat4Equal(const glm::mat4 &a,const glm::mat4 &b)
    {
        return a==b;
    }

    static bool IsIdentity(const glm::mat4 &m)
    {
        return Mat4Equal(m,glm::mat4(1.0f));
    }

    int32_t Model::internBounds(const BoundingVolumes &volumes)
    {
        for(size_t i=0; i<bounds.size(); ++i)
        {
            const auto &e=bounds[i];
            if(e.aabb.min==volumes.aabb.min&&
               e.aabb.max==volumes.aabb.max&&
               e.obb.center==volumes.obb.center&&
               e.obb.axisX==volumes.obb.axisX&&
               e.obb.axisY==volumes.obb.axisY&&
               e.obb.axisZ==volumes.obb.axisZ&&
               e.obb.halfSize==volumes.obb.halfSize&&
               e.sphere.center==volumes.sphere.center&&
               e.sphere.radius==volumes.sphere.radius)
            {
                return static_cast<int32_t>(i);
            }
        }
        bounds.push_back(volumes);
        return static_cast<int32_t>(bounds.size()-1);
    }

    int32_t Model::internTRS(const TRS &t)
    {
        for(size_t i=0; i<trsPool.size(); ++i)
        {
            const auto &e=trsPool[i];
            if(e.translation==t.translation&&
               e.rotation==t.rotation&&
               e.scale==t.scale)
            {
                return static_cast<int32_t>(i);
            }
        }
        trsPool.push_back(t);
        return static_cast<int32_t>(trsPool.size()-1);
    }

    int32_t Model::internMatrix(const glm::mat4 &m)
    {
        if(IsIdentity(m)) return -1;
        for(size_t i=0; i<matrixData.size(); ++i)
            if(Mat4Equal(matrixData[i],m))
                return static_cast<int32_t>(i);
        matrixData.push_back(m);
        return static_cast<int32_t>(matrixData.size()-1);
    }
} // namespace pure
