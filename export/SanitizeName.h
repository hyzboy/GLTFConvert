#pragma once
#include <string>

namespace exporters
{
    // Unified name sanitization utility.
    // Keeps [A-Za-z0-9._-]; others -> '_' ; collapses consecutive '_' ; trims
    // leading/trailing '.' or '_' ; guarantees non-empty ("unnamed") ; avoids
    // Windows reserved device names by appending '_'.
    std::string SanitizeName(std::string n);
}
