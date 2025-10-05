#pragma once

#include <string>
#include <cstddef>
#include "common/IndexType.h"

namespace pure
{

    struct GeometryIndicesMeta
    {
        std::size_t count=0;            // number of indices
        std::string componentType;        // e.g. UNSIGNED_SHORT
        IndexType indexType=IndexType::ERR; // resolved enum type
    };

} // namespace pure
