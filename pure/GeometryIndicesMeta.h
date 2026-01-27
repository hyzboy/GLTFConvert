#pragma once

#include <cstddef>
#include "common/IndexType.h"

namespace pure
{

    struct GeometryIndicesMeta
    {
        std::size_t count=0;              // number of indices
        IndexType indexType=IndexType::ERR; // resolved enum type
    };

} // namespace pure
