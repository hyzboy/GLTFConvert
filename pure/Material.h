#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include <unordered_map>

namespace pure
{
    struct Model; // forward

    struct Material
    {
        std::string type;
        std::string name;

        virtual ~Material() = default;
        // toJson for export: derived materials may use model and resource remap tables
        virtual nlohmann::json toJson(const Model &model,
                                       const std::unordered_map<std::size_t,int32_t> &texRemap,
                                       const std::unordered_map<std::size_t,int32_t> &imgRemap,
                                       const std::unordered_map<std::size_t,int32_t> &sampRemap,
                                       const std::string &baseName) const = 0;
    };
}
