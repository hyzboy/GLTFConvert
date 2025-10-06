#pragma once

#include <filesystem>
#include "gltf/GLTFModel.h"

namespace exporters
{
    bool ExportPureModel(const GLTFModel &model,const std::filesystem::path &outDir);

} // namespace exporters
