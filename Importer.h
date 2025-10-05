#pragma once

#include <filesystem>
#include "gltf/Model.h"

namespace importers {

bool ImportFastGLTF(const std::filesystem::path& inputPath, gltf::GLTFModel& outModel);

} // namespace importers
