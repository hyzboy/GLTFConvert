#pragma once
#include <filesystem>
namespace pure { struct Model; }

namespace exporters {
    bool ExportImages(const pure::Model &model, const std::filesystem::path &targetDir);
}
