#pragma once

#include <fbxsdk.h>
#include "fbx/FBXModel.h"

namespace fbx {
    void ProcessMesh(FbxMesh* mesh, FBXModel& model);
}
