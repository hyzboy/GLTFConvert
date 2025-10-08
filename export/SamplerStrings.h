#pragma once
#include <string>

namespace exporters
{
    // Convert GL wrap mode enum values to readable strings.
    std::string WrapModeToString(int v);
    // Convert GL filter enum values to readable strings.
    std::string FilterToString(int v);
}
