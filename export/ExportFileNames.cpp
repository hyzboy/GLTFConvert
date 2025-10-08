#include "ExportFileNames.h"

#include <filesystem>

namespace exporters
{
    // ------------------------------------------------------------
    // File name builders (SanitizeName moved to SanitizeName.cpp)
    // ------------------------------------------------------------
    std::string MakeGeometryFileName(const std::string &baseName, int32_t geometryIndex)
    {
        return baseName + "." + std::to_string(geometryIndex) + ".geometry";
    }

    std::string MakeMaterialFileName(const std::string &baseName, const std::string &matName, int32_t materialIndex)
    {   
        std::string front = baseName + "." + std::to_string(materialIndex) + ".";

        if (!matName.empty())
            return front+SanitizeName(matName) + ".material";

        return front+".material";
    }

    std::string MakeImageFileName(const std::string &baseName, const std::string &imageName, int32_t imageIndex, const std::string &ext)
    {
        if (!imageName.empty())
            return baseName + "." + SanitizeName(imageName) + ext;
        return baseName + ".image." + std::to_string(imageIndex) + ext;
    }

    std::string MakeTexturesJsonFileName(const std::string &baseName, const std::string &sceneName)
    {
        return baseName + "." + sceneName + ".textures";
    }

    std::string MakeSceneJsonFileName(const std::string &baseName, const std::string &sceneName)
    {
        return baseName + "." + sceneName + ".json";
    }

    std::string MakeScenePackFileName(const std::string &baseName, const std::string &sceneName)
    {
        return baseName + "." + sceneName + ".scene";
    }

    std::string MakeMeshFileName(const std::string &baseName, const std::string &meshName, int32_t meshIndex)
    {
        std::string front = baseName + "." + std::to_string(meshIndex) + ".";
        if (!meshName.empty())
            return front + SanitizeName(meshName) + ".mesh";
        return front + ".mesh"; // results in baseName.index..mesh when unnamed (keeps previous behavior)
    }
}
