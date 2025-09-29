#pragma once

#include <cstddef>
#include <optional>

namespace pure {

struct SubMesh {
    std::size_t geometry = static_cast<std::size_t>(-1); // index into Model::geometry
    std::optional<std::size_t> material; // index into Model::materials or null
};

} // namespace pure
