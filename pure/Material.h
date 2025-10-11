#pragma once
#include <string>
#include <nlohmann/json.hpp>

namespace pure
{
    struct Model; // forward

    struct Material
    {
        std::string type;
        std::string name;

        virtual ~Material() = default;
        virtual nlohmann::json toJson() const = 0;
    };
}
