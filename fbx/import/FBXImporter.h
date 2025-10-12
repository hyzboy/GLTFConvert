#pragma once

#include <filesystem>
#include "fbx/FBXModel.h"

namespace fbx
{
    bool ImportFBX(const std::filesystem::path &inputPath, FBXModel &outModel);
    // Allow using 8-bit indices when converting. Default is false.
    extern bool g_allow_u8_indices;
    void SetAllowU8Indices(bool allow);
}//namespace fbx
