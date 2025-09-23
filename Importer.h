#pragma once

#include <filesystem>
#include "PureGLTF.h"

namespace importers {

// Import from fastgltf Asset into our puregltf::Model
bool ImportFastGLTF(const std::filesystem::path& inputPath, puregltf::Model& outModel);

} // namespace importers
