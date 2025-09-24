#include"BoundingBox.h"

// Helper: sphere from AABB (center = mid, radius = half diagonal)
BoundingSphere SphereFromAABB(const AABB &a) {
    BoundingSphere s; s.reset();
    if(!a.empty()) {
        s.center = (a.min + a.max) * 0.5;
        s.radius = glm::length(a.max - s.center);
    }
    return s;
}

// Helper: sphere from points (center = average, radius = max distance to center)
BoundingSphere SphereFromPoints(const std::vector<glm::dvec3> &pts) {
    BoundingSphere s; s.reset();
    if(pts.empty()) return s;
    glm::dvec3 c(0.0);
    for(const auto &p : pts) c += p;
    c /= static_cast<double>(pts.size());
    double r = 0.0;
    for(const auto &p : pts) r = std::max(r,glm::length(p - c));
    s.center = c;
    s.radius = r;
    return s;
}
