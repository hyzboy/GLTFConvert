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

    glm::mat4 GetNodeWorldMatrix(const Model &m,const MeshNode &n)
    {
        if(n.worldMatrixIndexPlusOne==0) return glm::mat4(1.0f);
        auto idx=n.worldMatrixIndexPlusOne-1;
        return m.matrixData[(size_t)idx];
    }

    glm::mat4 GetNodeLocalMatrix(const Model &m,const MeshNode &n)
    {
        if(n.localMatrixIndexPlusOne==0) return glm::mat4(1.0f);
        auto idx=n.localMatrixIndexPlusOne-1;
        return m.matrixData[(size_t)idx];
    }

    const std::optional<TRS> &GetNodeTRS(const Model &m,const MeshNode &n)
    {
        if(n.trsIndexPlusOne==0)
        {
            static const std::optional<TRS> kEmpty{};
            return kEmpty;
        }
        auto idx=n.trsIndexPlusOne-1;
        struct Holder { std::optional<TRS> opt; };
        static thread_local Holder holder;
        holder.opt=m.trsPool[(size_t)idx];
        return holder.opt;
    }

} // namespace pure
