#pragma once

#include <filesystem>
#include "gltf/GLTFModel.h"

namespace gltf
{
    bool ImportFastGLTF(const std::filesystem::path &inputPath,GLTFModel &outModel);
}//namespace gltf
