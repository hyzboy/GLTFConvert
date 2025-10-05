#pragma once

#include<string>
#include<vector>
#include<optional>
#include"math/BoundingBox.h"
#include"common/VKFormat.h"
#include"common/VertexAttrib.h"
#include"common/IndexType.h"
#include"common/PrimitiveType.h"
#include"pure/BoundsIndex.h"

namespace pure
{
    struct GeometryAttribute
    {
        int64_t id = 0; // index in the attributes array
        std::string name;
        std::size_t count = 0;
        std::string componentType; // e.g. FLOAT
        std::string type; // e.g. VEC3

        VkFormat format;

        std::vector<std::byte> data; // raw attribute data
    };

    struct GeometryIndicesMeta {
        std::size_t count = 0;
        std::string componentType; // e.g. UNSIGNED_SHORT

        IndexType indexType = IndexType::ERR;
    };

    struct Geometry {
        std::string mode; // e.g. TRIANGLES

        PrimitiveType primitiveType = PrimitiveType::Triangles;

        std::vector<GeometryAttribute> attributes; // includes raw data
        // Index into Model::bounds pool (combined bounding info for this geometry)
        int32_t boundsIndex = kInvalidBoundsIndex;
        std::optional<std::vector<std::byte>> indicesData; // raw index buffer if present
        std::optional<GeometryIndicesMeta> indices; // metadata only
        // Optional double-precision decoded POSITION data (local space)
        std::optional<std::vector<glm::dvec3>> positions;
    };

    bool SaveGeometry(const Geometry &geometry,const BoundingBox &,const std::string &filename);
}//namespace pure
