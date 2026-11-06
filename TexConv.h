#pragma once
#include <filesystem>

namespace texconv
{
    struct ConvertOptions
    {
        bool normal_map = false;
        bool gen_mipmaps = true;
    };

    // Set explicit .ini configuration file before detection
    void SetCustomConfigFile(const std::filesystem::path &cfgPath);
    // Perform one-time detection; returns true if TexConv executable exists beside current program.
    bool Initialize(std::filesystem::path *outPath=nullptr);
    // Returns true if TexConv is available (will trigger detection on first call).
    bool IsAvailable();
    // Returns path to TexConv executable (empty path if unavailable).
    const std::filesystem::path & Path();
    // Attempt to convert (copy with processing) using TexConv; returns true on success.
    // dst is output file (TexConv will be invoked with /mip /out:dst src, plus /normal if options.normal_map is true)
    bool Convert(const std::filesystem::path &src, const std::filesystem::path &dst, const ConvertOptions &options = {});
}
