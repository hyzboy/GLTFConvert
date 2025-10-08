#pragma once
#include <string>
#include <filesystem>
#include <cstdint>
#include "pure/Model.h"

namespace exporters
{
    // Sanitize an arbitrary string into a safe filename component.
    // Keeps [A-Za-z0-9._-]; replaces others with '_'; collapses duplicates; trims leading/trailing '.'/'_';
    // guarantees non-empty; appends '_' if matches Windows reserved device name.
    std::string SanitizeName(std::string n);

    inline std::string GetBaseName(const pure::Model &model)
    {
        if(model.gltf_source.empty()) return std::string("scene");
        return std::filesystem::path(model.gltf_source).stem().string();
    }

    inline std::string MakeGeometryFileName(const std::string &baseName, int32_t geometryIndex)
    {
        return baseName + "." + std::to_string(geometryIndex) + ".geometry";
    }

    inline std::string MakeMaterialFileName(const std::string &baseName, const std::string &matName, int32_t materialIndex)
    {
        if(!matName.empty())
            return baseName + "." + SanitizeName(matName) + ".material";
        return baseName + ".material." + std::to_string(materialIndex);
    }

    inline std::string MakeImageFileName(const std::string &baseName, const std::string &imageName, int32_t imageIndex, const std::string &ext)
    {
        if(!imageName.empty()) return baseName + "." + SanitizeName(imageName) + ext;
        return baseName + ".image." + std::to_string(imageIndex) + ext;
    }

    inline std::string MakeTexturesJsonFileName(const std::string &baseName, const std::string &sceneName)
    {
        return baseName + "." + sceneName + ".textures";
    }

    inline std::string MakeSceneJsonFileName(const std::string &baseName, const std::string &sceneName)
    {
        return baseName + "." + sceneName + ".json";
    }

    inline std::string MakeScenePackFileName(const std::string &baseName, const std::string &sceneName)
    {
        return baseName + "." + sceneName + ".scene";
    }

    inline std::string MakeMeshFileName(const std::string &/*baseName*/, const std::string &meshName, int32_t meshIndex)
    {
        if(!meshName.empty())
            return std::to_string(meshIndex) + "." + SanitizeName(meshName) + ".mesh";
        return std::to_string(meshIndex) + ".mesh";
    }
}
