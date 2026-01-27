#include"math/BoundingVolumes.h"

void BoundingVolumes::Pack(PackedBounds *packed) const
{
    if(!packed)
        return;

    packed->aabbMin[0] = aabb.min.x;
    packed->aabbMin[1] = aabb.min.y;
    packed->aabbMin[2] = aabb.min.z;
    packed->aabbMax[0] = aabb.max.x;
    packed->aabbMax[1] = aabb.max.y;
    packed->aabbMax[2] = aabb.max.z;

    packed->obbCenter[0] = obb.center.x;
    packed->obbCenter[1] = obb.center.y;
    packed->obbCenter[2] = obb.center.z;

    packed->obbAxisX[0] = obb.axisX.x;
    packed->obbAxisX[1] = obb.axisX.y;
    packed->obbAxisX[2] = obb.axisX.z;

    packed->obbAxisY[0] = obb.axisY.x;
    packed->obbAxisY[1] = obb.axisY.y;
    packed->obbAxisY[2] = obb.axisY.z;

    packed->obbAxisZ[0] = obb.axisZ.x;
    packed->obbAxisZ[1] = obb.axisZ.y;
    packed->obbAxisZ[2] = obb.axisZ.z;

    packed->obbHalfSize[0] = obb.halfSize.x;
    packed->obbHalfSize[1] = obb.halfSize.y;
    packed->obbHalfSize[2] = obb.halfSize.z;

    packed->sphereCenter[0] = sphere.center.x;
    packed->sphereCenter[1] = sphere.center.y;
    packed->sphereCenter[2] = sphere.center.z;
    packed->sphereRadius = sphere.radius;
}
