#pragma once
#include <string>

namespace exporters {
// Return file extension for a given image mime type; unknown -> .bin
std::string GuessImageExtension(const std::string &mime);
}
