#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <glm/glm.hpp>

#include "SceneLocal.h" // for pure::SceneLocal
#include "StaticMesh.h"

namespace exporters {

// Refactored: now directly consumes a SceneLocal instance instead of individual pieces
bool WriteSceneBinary(
    const std::filesystem::path& sceneDir,
    const std::string& baseName,
    const pure::SceneLocal& sceneLocal);

} // namespace exporters
