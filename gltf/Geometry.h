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

struct GLTFGeometry
{
    PrimitiveType primitiveType=PrimitiveType::Triangles;

    struct GLTFGeometryAttribute
    {
        std::string name;
        std::size_t count=0;
        VkFormat format{}; // Vulkan format

        std::vector<std::byte> data; // raw data as-is
        std::optional<std::size_t> accessorIndex; // Source glTF accessor index
    };

    std::vector<GLTFGeometryAttribute> attributes;
    std::optional<std::vector<std::byte>> indices; // raw index data
    std::optional<std::size_t> indexCount;         // number of indices

    IndexType indexType=IndexType::ERR;
    std::optional<std::size_t> indicesAccessorIndex; // source accessor index

    ::AABB localAABB; // computed from POSITION if present
};
