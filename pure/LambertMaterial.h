#pragma once
#include "pure/Material.h"
#include "pure/FBXMaterial.h"
#include <optional>

namespace pure
{
    struct LambertMaterial : public Material
    {
        float diffuse[3] = {1.f,1.f,1.f};
        std::optional<TextureRef> diffuseTexture;
        float ambient[3] = {0.f,0.f,0.f};
        float emissive[3] = {0.f,0.f,0.f};

        LambertMaterial() { type = "Lambert"; }

        nlohmann::json toJson() const override
        {
            nlohmann::json j;
            j["name"] = name;
            j["type"] = type;
            j["diffuse"] = { diffuse[0], diffuse[1], diffuse[2] };
            return j;
        }
    };
}
