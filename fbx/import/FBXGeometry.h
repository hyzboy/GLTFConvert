#pragma once

// Forward-declare FBX SDK types in the fbxsdk namespace to avoid including heavy SDK headers in headers.
// Implementation files should include <fbxsdk.h> to get full definitions.
namespace fbxsdk { class FbxMesh; class FbxNode; }

#include <vector>
#include "fbx/FBXModel.h"

namespace fbx {
    void ProcessMesh(fbxsdk::FbxMesh* mesh, fbxsdk::FbxNode* node, FBXModel& model, const std::vector<int>& materialMap);
    void Expand_ByControlPoint(fbxsdk::FbxMesh* mesh, FBXModel& model, const std::vector<int> &materialMap);
    void Expand_ByPolygonVertex(fbxsdk::FbxMesh* mesh, FBXModel& model, const std::vector<int> &materialMap);
    void Expand_ByPolygon(fbxsdk::FbxMesh* mesh, FBXModel& model, const std::vector<int> &materialMap);
}
