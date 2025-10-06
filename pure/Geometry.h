#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstddef>
#include <glm/glm.hpp>

#include "common/PrimitiveType.h"
#include "pure/BoundsIndex.h"
#include "math/BoundingVolumes.h"
#include "common/GeometryAttribute.h" // moved from pure/
#include "pure/GeometryIndicesMeta.h"

namespace pure
{
    struct Geometry
    {
        std::string mode; // e.g. TRIANGLES
        PrimitiveType primitiveType=PrimitiveType::Triangles;

        std::vector<GeometryAttribute> attributes; // vertex attributes
        int32_t boundsIndex=kInvalidBoundsIndex; // index into global bounds pool

        std::optional<std::vector<std::byte>> indicesData; // raw index buffer if present
        std::optional<GeometryIndicesMeta> indices;        // metadata for indices

        // Optional decoded POSITION data (local space, stored as float precision)
        std::optional<std::vector<glm::vec3>> positions;
    };

    bool SaveGeometry(const Geometry &geometry,const BoundingVolumes &volumes,const std::string &filename);

} // namespace pure
