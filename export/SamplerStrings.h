#pragma once
#include <string>
#include "pure/Sampler.h"

namespace exporters
{
    // Convert wrap mode enum values to readable strings.
    std::string WrapModeToString(pure::WrapMode v);
    // Convert filter enum values to readable strings.
    std::string FilterToString(pure::FilterMode v);
}
