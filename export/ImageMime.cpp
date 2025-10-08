#include "ImageMime.h"

namespace exporters
{
    std::string GuessImageExtension(const std::string &mime)
    {
        // Common glTF official / extension-supported formats first
        if(mime=="image/png")           return ".png";
        if(mime=="image/jpeg")          return ".jpg";   // prefer .jpg
        if(mime=="image/ktx2")          return ".ktx2";
        if(mime=="image/vnd-ms.dds")    return ".dds";   // non-standard, used in some workflows
        if(mime=="image/webp")          return ".webp";

        // Additional common image mime types (not always part of glTF core spec but useful for exporters)
        if(mime=="image/bmp" || mime=="image/x-ms-bmp")            return ".bmp";
        // TGA has no registered official mime; various non-standard types appear in the wild
        if(mime=="image/tga" || mime=="image/x-tga" || mime=="image/x-targa" || mime=="application/x-tga") return ".tga";
        if(mime=="image/gif")                                      return ".gif";
        if(mime=="image/tiff" || mime=="image/x-tiff")            return ".tiff"; // could also choose .tif
        if(mime=="image/x-exr" || mime=="image/exr")              return ".exr";
        if(mime=="image/vnd.radiance" || mime=="image/x-radiance") return ".hdr"; // Radiance HDR
        if(mime=="image/heic" || mime=="image/heif")              return ".heic"; // HEIF family (choose .heic)
        if(mime=="image/avif")                                     return ".avif";

        return ".bin"; // fallback for unknown binary blob
    }
}
