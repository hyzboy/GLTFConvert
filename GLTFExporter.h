#pragma once
#include <filesystem>

namespace gltf {
// Export a glTF/glb to custom TOML + per-primitive binary blobs.
// outDir: if empty, export next to the source file.
bool ExportToTOML(const std::filesystem::path& inputPath,
                  const std::filesystem::path& outDir = {});
}
