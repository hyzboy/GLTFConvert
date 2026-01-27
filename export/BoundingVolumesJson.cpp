#include "BoundingVolumesJson.h"

nlohmann::json exporters::BoundingVolumesToJson(const BoundingVolumes &bv)
{
    nlohmann::json jb;
    if (!bv.emptyAABB())
    {
        jb["aabbMin"] = { bv.aabb.min.x, bv.aabb.min.y, bv.aabb.min.z };
        jb["aabbMax"] = { bv.aabb.max.x, bv.aabb.max.y, bv.aabb.max.z };
    }
    if (!bv.emptyOBB())
    {
        nlohmann::json jobb;
        jobb["center"] = { bv.obb.center.x, bv.obb.center.y, bv.obb.center.z };
        jobb["axisX"] = { bv.obb.axisX.x, bv.obb.axisX.y, bv.obb.axisX.z };
        jobb["axisY"] = { bv.obb.axisY.x, bv.obb.axisY.y, bv.obb.axisY.z };
        jobb["axisZ"] = { bv.obb.axisZ.x, bv.obb.axisZ.y, bv.obb.axisZ.z };
        jobb["halfSize"] = { bv.obb.halfSize.x, bv.obb.halfSize.y, bv.obb.halfSize.z };
        jb["obb"] = jobb;
    }
    if (!bv.emptySphere())
    {
        jb["sphereCenter"] = { bv.sphere.center.x, bv.sphere.center.y, bv.sphere.center.z };
        jb["sphereRadius"] = bv.sphere.radius;
    }
    return jb;
}
