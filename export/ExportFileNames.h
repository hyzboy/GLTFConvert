#pragma once
#include <string>
#include <cstdint>

namespace exporters
{
    std::string SanitizeName(std::string n);
    std::string MakeGeometryFileName(const std::string &baseName, int32_t geometryIndex, int32_t totalGeometryCount = -1);
    std::string MakeMaterialFileName(const std::string &baseName, const std::string &matName, int32_t materialIndex, int32_t totalMaterialCount = -1);
    std::string MakeImageFileName(const std::string &baseName, const std::string &imageName, int32_t imageIndex, const std::string &ext);
    std::string MakeTexturesJsonFileName(const std::string &baseName, const std::string &sceneName);
    std::string MakeSceneJsonFileName(const std::string &baseName, const std::string &sceneName);
    std::string MakeScenePackFileName(const std::string &baseName, const std::string &sceneName);
    std::string MakeMeshFileName(const std::string &baseName, const std::string &meshName, int32_t meshIndex, int32_t totalMeshCount = -1);
}
