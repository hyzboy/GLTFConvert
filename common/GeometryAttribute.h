#pragma once

#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <glm/glm.hpp>
#include "common/VKFormat.h"

// GeometryAttribute moved to global namespace (previously pure::GeometryAttribute)
struct GeometryAttribute
{
    std::string name;
    std::size_t count = 0;
    // componentType and type removed; VkFormat encodes layout now
    VkFormat format{};                 // Vulkan format used
    std::vector<std::byte> data;       // raw attribute data
};
