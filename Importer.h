#pragma once

#include <filesystem>
#include "gltf/Model.h"

namespace importers
{

    bool ImportFastGLTF(const std::filesystem::path &inputPath,GLTFModel &outModel);

} // namespace importers
