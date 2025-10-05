#pragma once

#include <filesystem>
#include "gltf/Model.h"

namespace importers {

// Import from fastgltf Asset into our gltf::Model
bool ImportFastGLTF(const std::filesystem::path& inputPath, gltf::Model& outModel);

} // namespace importers
