#pragma once

#include <filesystem>
#include "gltf/GLTFModel.h"

namespace importers
{
    bool ImportFastGLTF(const std::filesystem::path &inputPath,GLTFModel &outModel);
}//namespace importers
