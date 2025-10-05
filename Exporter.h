#pragma once

#include <filesystem>
#include "gltf/Model.h"

namespace exporters {

// Export the pure Model into our JSON + binary attribute blobs layout
bool ExportPureModel(const gltf::Model& model, const std::filesystem::path& outDir);

} // namespace exporters
