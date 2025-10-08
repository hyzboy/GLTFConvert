#include "ImageMime.h"

namespace exporters {
std::string GuessImageExtension(const std::string &mime)
{
    if (mime == "image/png")        return ".png";
    if (mime == "image/jpeg")       return ".jpg";
    if (mime == "image/ktx2")       return ".ktx2";
    if (mime == "image/vnd-ms.dds") return ".dds";
    if (mime == "image/webp")       return ".webp";
    return ".bin";
}
}
