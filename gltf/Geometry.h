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
#include "common/GeometryAttribute.h" // base attribute definition

struct GLTFGeometry
{
    PrimitiveType primitiveType = PrimitiveType::Triangles;

    // Extend shared GeometryAttribute with glTF-specific metadata
    struct GLTFGeometryAttribute : public GeometryAttribute
    {
        std::optional<std::size_t> accessorIndex; // Source glTF accessor index
    };

    std::vector<GLTFGeometryAttribute> attributes;          // vertex attributes with source info
    std::optional<std::vector<std::byte>> indices;          // raw index data
    std::optional<std::size_t> indexCount;                  // number of indices

    IndexType indexType = IndexType::ERR;
    std::optional<std::size_t> indicesAccessorIndex;        // source accessor index for indices buffer

    ::AABB localAABB; // computed from POSITION if present
};
