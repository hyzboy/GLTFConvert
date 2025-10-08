#pragma once
#include <filesystem>
#include <vector>
#include <cstddef>
namespace pure { struct Model; }

namespace exporters
{
    // If allowedImageIndices is non-null, only export images whose original index is contained in it.
    bool ExportImages(const pure::Model &model,const std::filesystem::path &targetDir,const std::vector<std::size_t> *allowedImageIndices=nullptr);
}
