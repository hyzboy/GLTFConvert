#pragma once
#include <vector>
#include <cstdint>

namespace gltf
{
    // Mapping from glTF primitive indices to unique geometry entries
    struct UniqueGeometryMapping
    {
        std::vector<int32_t> uniqueRepGeomPrimIdx; // representative primitive index for each unique geometry
        std::vector<int32_t> geomIndexOfPrim;      // primitive -> unique geometry index
    };
}
