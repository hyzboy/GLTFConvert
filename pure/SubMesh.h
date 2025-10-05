#pragma once

#include <cstddef>
#include <optional>
#include <cstdint>

namespace pure
{

    struct SubMesh
    {
        int32_t geometry=static_cast<int32_t>(-1); // index into Model::geometry
        std::optional<int32_t> material; // index into Model::materials or null
    };

} // namespace pure
