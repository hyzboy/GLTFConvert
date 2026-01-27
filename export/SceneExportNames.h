#pragma once

#include <unordered_map>
#include <string>
#include <vector>

namespace exporters
{
    // Add or retrieve a deduplicated name from the table, return its index (-1 if empty).
    int32_t GetOrAddName(std::unordered_map<std::string, int32_t> &map,
                         std::vector<std::string> &table,
                         const std::string &name);
}
