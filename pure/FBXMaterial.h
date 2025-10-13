#pragma once
#include "pure/Material.h"
#include "pure/PBRMaterial.h"
#include <nlohmann/json.hpp>
#include <vector>
#include <unordered_map>

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

    nlohmann::json toJson(const ::pure::Model & /*model*/,
                           const std::unordered_map<std::size_t,int32_t> &texRemap,
                           const std::unordered_map<std::size_t,int32_t> & /*imgRemap*/,
                           const std::unordered_map<std::size_t,int32_t> & /*sampRemap*/,
                           const std::string & /*baseName*/) const override
    {
        nlohmann::json j;
        j["name"] = name;
        j["type"] = type;
        j["impl"] = impl;
        j["raw"] = raw;

        if (!textures.empty()) {
            nlohmann::json arr = nlohmann::json::array();
            for (const auto &t : textures) {
                nlohmann::json jt = nlohmann::json::object();
                if (t.index) {
                    auto it = texRemap.find(*t.index);
                    if (it != texRemap.end()) jt["texture"] = it->second;
                }
                if (t.texCoord != 0) jt["texCoord"] = t.texCoord;
                if (t.scale != 1.0f) jt["scale"] = t.scale;
                if (t.strength != 1.0f) jt["strength"] = t.strength;
                arr.push_back(std::move(jt));
            }
            j["textures"] = std::move(arr);
        }

        return j;
    }
};

}
