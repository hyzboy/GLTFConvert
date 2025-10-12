#pragma once
#include "pure/Material.h"
#include "pure/PBRMaterial.h"
#include <nlohmann/json.hpp>
#include <vector>

namespace pure {

struct FBXMaterial : public Material
{
    std::string impl; // e.g., "Phong", "Lambert", "SpecGloss", "PBR", "Custom"
    nlohmann::json raw; // raw properties
    std::vector<TextureRef> textures;

    FBXMaterial()
    {
        type = "FBX";
    }

    nlohmann::json toJson() const override
    {
        nlohmann::json j;
        j["name"] = name;
        j["type"] = type;
        j["impl"] = impl;
        j["raw"] = raw;
        return j;
    }
};

}
