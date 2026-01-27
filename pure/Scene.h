#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "pure/BoundsIndex.h"

namespace pure
{
    struct Scene
    {
        std::string name;
        std::vector<int32_t> nodes;
    };
}
