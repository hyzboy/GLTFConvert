#pragma once
#include <filesystem>
namespace exporters { struct SceneExportData; bool WriteSceneJson(const SceneExportData &data, const std::filesystem::path &filePath); }
