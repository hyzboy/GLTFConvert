#pragma once

#include <fbxsdk.h>
#include "fbx/FBXModel.h"

namespace fbx {
    void ProcessMesh(FbxMesh* mesh, FbxNode* node, FBXModel& model);
    void Expand_ByControlPoint(FbxMesh* mesh, FBXModel& model, const std::vector<int> &materialMap);
    void Expand_ByPolygonVertex(FbxMesh* mesh, FBXModel& model, const std::vector<int> &materialMap);
    void Expand_ByPolygon(FbxMesh* mesh, FBXModel& model, const std::vector<int> &materialMap);
}
