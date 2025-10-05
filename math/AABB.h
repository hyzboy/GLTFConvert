#pragma once

#include <limits>
#include <glm/glm.hpp>

// Axis-aligned bounding box in double precision, global namespace
struct AABB {
    glm::dvec3 min{ std::numeric_limits<double>::infinity() };
    glm::dvec3 max{ -std::numeric_limits<double>::infinity() };

    void reset() {
        min = { std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity() };
        max = { -std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity() };
    }

    bool empty() const {
        return !(min.x <= max.x && min.y <= max.y && min.z <= max.z);
    }

    void include(const glm::dvec3& p) {
        min = glm::min(min, p);
        max = glm::max(max, p);
    }

    void merge(const AABB& other) {
        if (other.empty()) return;
        include(other.min);
        include(other.max);
    }

    AABB transformed(const glm::dmat4& m) const {
        if (empty()) return *this;
        // transform 8 corners and recompute
        const glm::dvec3 c[8] = {
            {min.x, min.y, min.z}, {max.x, min.y, min.z}, {min.x, max.y, min.z}, {min.x, min.y, max.z},
            {max.x, max.y, min.z}, {max.x, min.y, max.z}, {min.x, max.y, max.z}, {max.x, max.y, max.z}
        };
        AABB out; out.reset();
        for (auto& p : c) {
            glm::dvec4 tp = m * glm::dvec4(p, 1.0);
            out.include(glm::dvec3(tp));
        }
        return out;
    }
};
