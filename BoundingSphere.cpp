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

void Write(std::ostream &ofs,const BoundingBox &bounds)
{
    {
        float fmin[3];
        float fmax[3];
        fmin[0] = static_cast<float>(bounds.aabb.min.x);
        fmin[1] = static_cast<float>(bounds.aabb.min.y);
        fmin[2] = static_cast<float>(bounds.aabb.min.z);
        fmax[0] = static_cast<float>(bounds.aabb.max.x);
        fmax[1] = static_cast<float>(bounds.aabb.max.y);
        fmax[2] = static_cast<float>(bounds.aabb.max.z);
        ofs.write(reinterpret_cast<const char *>(fmin),sizeof(fmin));
        ofs.write(reinterpret_cast<const char *>(fmax),sizeof(fmax));
    }

    {
        float center[3];
        float axisX[3];
        float axisY[3];
        float axisZ[3];
        float halfSize[3];

        center[0] = static_cast<float>(bounds.obb.center.x);
        center[1] = static_cast<float>(bounds.obb.center.y);
        center[2] = static_cast<float>(bounds.obb.center.z);

        axisX[0] = static_cast<float>(bounds.obb.axisX.x);
        axisX[1] = static_cast<float>(bounds.obb.axisX.y);
        axisX[2] = static_cast<float>(bounds.obb.axisX.z);

        axisY[0] = static_cast<float>(bounds.obb.axisY.x);
        axisY[1] = static_cast<float>(bounds.obb.axisY.y);
        axisY[2] = static_cast<float>(bounds.obb.axisY.z);

        axisZ[0] = static_cast<float>(bounds.obb.axisZ.x);
        axisZ[1] = static_cast<float>(bounds.obb.axisZ.y);
        axisZ[2] = static_cast<float>(bounds.obb.axisZ.z);

        halfSize[0] = static_cast<float>(bounds.obb.halfSize.x);
        halfSize[1] = static_cast<float>(bounds.obb.halfSize.y);
        halfSize[2] = static_cast<float>(bounds.obb.halfSize.z);

        ofs.write(reinterpret_cast<const char *>(center),sizeof(center));
        ofs.write(reinterpret_cast<const char *>(axisX),sizeof(axisX));
        ofs.write(reinterpret_cast<const char *>(axisY),sizeof(axisY));
        ofs.write(reinterpret_cast<const char *>(axisZ),sizeof(axisZ));
        ofs.write(reinterpret_cast<const char *>(halfSize),sizeof(halfSize));
    }

    {
        float center[3];
        float radius;
        center[0] = static_cast<float>(bounds.sphere.center.x);
        center[1] = static_cast<float>(bounds.sphere.center.y);
        center[2] = static_cast<float>(bounds.sphere.center.z);
        radius = static_cast<float>(bounds.sphere.radius);
        ofs.write(reinterpret_cast<const char *>(center),sizeof(center));
        ofs.write(reinterpret_cast<const char *>(&radius),sizeof(radius));
    }
}
