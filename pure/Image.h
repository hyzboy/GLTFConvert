#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstddef>

namespace pure
{
    struct Image
    {
        std::string name;
        std::string mimeType;
        std::optional<std::vector<std::byte>> data; // embedded data
        std::optional<std::string> uri; // external URI
    };
}
