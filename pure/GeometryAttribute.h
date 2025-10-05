#pragma once

#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <glm/glm.hpp>
#include "common/VKFormat.h"

namespace pure {

struct GeometryAttribute {
    int64_t id = 0; // index in the attributes array
    std::string name;
    std::size_t count = 0;
    std::string componentType; // e.g. FLOAT
    std::string type; // e.g. VEC3

    VkFormat format; // Vulkan format used

    std::vector<std::byte> data; // raw attribute data
};

} // namespace pure
