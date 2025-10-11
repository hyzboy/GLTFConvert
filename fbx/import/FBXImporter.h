#pragma once

#include <filesystem>
#include "fbx/FBXModel.h"

namespace fbx
{
    bool ImportFBX(const std::filesystem::path &inputPath, FBXModel &outModel);
}//namespace fbx
