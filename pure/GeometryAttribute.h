#pragma once

#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <glm/glm.hpp>
#include "common/VKFormat.h"

namespace pure
{
    struct GeometryAttribute
    {
        uint8_t id=0; // index in the attributes array
        std::string name;
        std::size_t count=0;
        // componentType and type removed; VkFormat encodes layout now
        VkFormat format{}; // Vulkan format used
        std::vector<std::byte> data; // raw attribute data
    };

} // namespace pure
