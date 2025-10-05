#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "StaticMesh.h"

namespace exporters {

// Write a single Scene.bin that contains scene-local nodes, submeshes (geometry file names),
// matrices and TRS pools. BoundingBox and Material are not stored.
bool WriteSceneBinary(
    const std::filesystem::path& sceneDir,
    const std::string& sceneName,
    const std::vector<int32_t>& sceneRootIndices,
    const std::string& baseName,
    const std::vector<pure::MeshNode>& nodes,
    const std::vector<pure::SubMesh>& subMeshes,
    const std::vector<pure::MatrixEntry>& matrices,
    const std::vector<pure::MeshNodeTransform>& trs);

} // namespace exporters
