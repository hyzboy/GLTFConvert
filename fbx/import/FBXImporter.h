#pragma once

#include <filesystem>
#include "fbx/FBXModel.h"

namespace fbx
{
    bool ImportFBX(const std::filesystem::path &inputPath, FBXModel &outModel);
    // Allow using 8-bit indices when converting. Default is false.
    inline bool g_allow_u8_indices = false;
    inline void SetAllowU8Indices(bool allow) { g_allow_u8_indices = allow; }
}//namespace fbx
