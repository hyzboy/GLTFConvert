#pragma once
#include "pure/Material.h"
#include "pure/FBXMaterial.h"
#include <optional>
#include <unordered_map>

namespace pure
{
    struct PhongMaterial : public Material
    {
        float diffuse[3] = {1.f,1.f,1.f};
        float specular[3] = {1.f,1.f,1.f};
        float shininess = 30.f;
        std::optional<TextureRef> diffuseTexture;
        std::optional<TextureRef> specularTexture;

        PhongMaterial() { type = "Phong"; }

        nlohmann::json toJson(const ::pure::Model & /*model*/,
                               const std::unordered_map<std::size_t,int32_t> & /*texRemap*/,
                               const std::unordered_map<std::size_t,int32_t> & /*imgRemap*/,
                               const std::unordered_map<std::size_t,int32_t> & /*sampRemap*/,
                               const std::string & /*baseName*/) const override
        {
            nlohmann::json j;
            j["name"] = name;
            j["type"] = type;
            j["diffuse"] = { diffuse[0], diffuse[1], diffuse[2] };
            j["specular"] = { specular[0], specular[1], specular[2] };
            j["shininess"] = shininess;
            return j;
        }
    };
}
