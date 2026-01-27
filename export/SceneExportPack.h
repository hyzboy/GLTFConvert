#pragma once
#include <filesystem>
namespace exporters { struct SceneExportData; bool WriteScenePack(const SceneExportData &data, const std::filesystem::path &filePath); }
