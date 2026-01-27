#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstddef>
#include <glm/glm.hpp>

#include "common/PrimitiveType.h"
#include "pure/BoundsIndex.h"
#include "math/BoundingVolumes.h"
#include "common/GeometryAttribute.h"
#include "pure/GeometryIndicesMeta.h"

namespace pure
{
    struct Geometry
    {
        PrimitiveType                           primitiveType   =PrimitiveType::Triangles;

        std::vector<GeometryAttribute>          attributes;                                         // vertex attributes

        BoundingVolumes                         bounding_volume;

        std::optional<std::vector<std::byte>>   indicesData;                                        // raw index buffer if present
        std::optional<GeometryIndicesMeta>      indices;                                            // metadata for indices

        std::optional<std::vector<glm::vec3>>   positions;                                          // Optional decoded POSITION data (local space, stored as float precision)

        std::optional<int32_t>                  material;                                           // index into Model::materials
    };

    bool SaveGeometry(Geometry &geometry,const std::string &filename);
} // namespace pure
