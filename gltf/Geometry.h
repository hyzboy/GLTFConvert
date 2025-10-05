#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include "math/AABB.h"
#include "common/VKFormat.h"
#include "common/IndexType.h"
#include "common/PrimitiveType.h"

namespace gltf {

struct Geometry {
    std::string mode; // e.g. TRIANGLES

    PrimitiveType primitiveType = PrimitiveType::Triangles;

    struct Attribute {
        std::string name;
        std::size_t count = 0;
        std::string componentType; // e.g. FLOAT
        std::string type; // e.g. VEC3

        VkFormat format{}; // Vulkan format

        std::vector<std::byte> data; // raw data as-is
        std::optional<std::size_t> accessorIndex; // Source glTF accessor index
    };
    std::vector<Attribute> attributes;
    std::optional<std::vector<std::byte>> indices; // raw index data
    std::optional<std::size_t> indexCount;         // number of indices
    std::optional<std::string> indexComponentType; // e.g. UNSIGNED_SHORT

    IndexType indexType = IndexType::ERR;
    std::optional<std::size_t> indicesAccessorIndex; // source accessor index

    ::AABB localAABB; // computed from POSITION if present
};

} // namespace gltf
