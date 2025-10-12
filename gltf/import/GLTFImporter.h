#pragma once

#include <filesystem>
#include "gltf/GLTFModel.h"

namespace gltf
{
    bool ImportFastGLTF(const std::filesystem::path &inputPath,GLTFModel &outModel);
    // Allow enabling uint8 index buffers when importing. Default is false.
    void SetAllowU8Indices(bool allow);
    bool GetAllowU8Indices();
}//namespace gltf
