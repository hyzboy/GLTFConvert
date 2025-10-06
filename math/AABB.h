#pragma once

#include <limits>
#include <glm/glm.hpp>
#include <vector>

// Axis-aligned bounding box in single precision
struct AABB
{
    glm::vec3 min{ std::numeric_limits<float>::infinity() };
    glm::vec3 max{ -std::numeric_limits<float>::infinity() };

    void reset()
    {
        min={ std::numeric_limits<float>::infinity(),std::numeric_limits<float>::infinity(),std::numeric_limits<float>::infinity() };
        max={ -std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity() };
    }

    bool empty() const
    {
        return !(min.x<=max.x&&min.y<=max.y&&min.z<=max.z);
    }

    void include(const glm::vec3 &p)
    {
        min=glm::min(min,p);
        max=glm::max(max,p);
    }

    void include(const glm::vec4 &p)
    {
        glm::vec3 p3(p);

        min=glm::min(min,p3);
        max=glm::max(max,p3);
    }

    void merge(const AABB &other)
    {
        if(other.empty()) return;
        include(other.min);
        include(other.max);
    }

    AABB transformed(const glm::mat4 &m) const
    {
        if(empty()) return *this;
        const glm::vec3 c[8]={
            { min.x,min.y,min.z },{ max.x,min.y,min.z },{ min.x,max.y,min.z },{ min.x,min.y,max.z },
            { max.x,max.y,min.z },{ max.x,min.y,max.z },{ min.x,max.y,max.z },{ max.x,max.y,max.z }
        };
        AABB out; out.reset();
        for(const auto &p:c)
        {
            glm::vec4 tp=m*glm::vec4(p,1.0f);
            out.include(glm::vec3(tp));
        }
        return out;
    }

    template<typename T>
    static AABB fromPoints(const std::vector<T> &points)
    {
        AABB box;
        box.reset();
        for(const auto &p:points) box.include(p);
        return box;
    }
};
