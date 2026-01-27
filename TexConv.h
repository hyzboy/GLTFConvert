#pragma once
#include <filesystem>

namespace texconv
{
    // Perform one-time detection; returns true if TexConv executable exists beside current program.
    bool Initialize(std::filesystem::path *outPath=nullptr);
    // Returns true if TexConv is available (will trigger detection on first call).
    bool IsAvailable();
    // Returns path to TexConv executable (empty path if unavailable).
    const std::filesystem::path & Path();
    // Attempt to convert (copy with processing) using TexConv; returns true on success.
    // dst is output file (TexConv will be invoked with /mip /out:dst src)
    bool Convert(const std::filesystem::path &src, const std::filesystem::path &dst);
}
