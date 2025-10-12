#pragma once

#include <vector>
#include "fbx/FBXModel.h"

namespace fbxsdk { class FbxNode; }
namespace fbx {
    void BuildMaterialMap(fbxsdk::FbxNode* node, ::FBXModel& model, std::vector<int>& materialMap);
}
