#include "ExportImages.h"
#include "pure/Model.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_set>
#include <cstdlib>
#include "ExportFileNames.h"
#include "ImageMime.h" // added
#include "ImageUsage.h" // new usage inference
#include "TexConv.h" // texconv availability

namespace exporters
{
    // Helper: copy or convert external image file with usage info
    static bool CopyImageFileWithUsage(const std::filesystem::path &srcPath,
                                       const std::filesystem::path &dstPath,
                                       ImageUsage usage)
    {
        // Try TexConv convert first
        if(texconv::Convert(srcPath, dstPath))
        {
            return true;
        }

        // Fallback to plain file copy
        std::error_code ec;
        std::filesystem::copy_file(srcPath, dstPath, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec)
        {
            std::cerr << "[Export] Copy image failed: " << srcPath << " -> " << dstPath << " : " << ec.message() << "\n";
            return false;
        }
        std::cout << "[Export] Copied external image: " << srcPath << " -> " << dstPath << " (" << ImageUsageToString(usage) << ")\n";
        return true;
    }

    bool ExportImages(const pure::Model &model, const std::filesystem::path &targetDir, const std::vector<std::size_t> *allowedImageIndices)
    {
        if (model.images.empty()) return true;

        std::unordered_set<std::size_t> allowed;
        if (allowedImageIndices)
            allowed.insert(allowedImageIndices->begin(), allowedImageIndices->end());

        const std::string baseName = model.GetBaseName();

        // precompute usage classification
        std::vector<ImageUsage> usages; BuildImageUsage(model, usages);

        for (std::size_t i = 0; i < model.images.size(); ++i)
        {
            if (allowedImageIndices && !allowed.count(i))
                continue;

            const auto &img = model.images[i];
            std::string ext = GuessImageExtension(img.mimeType);
            std::string fileName = MakeImageFileName(baseName, img.name, static_cast<int32_t>(i), ext);
            // if image has no name, optionally inject usage before extension for readability
            if(img.name.empty())
            {
                const char *usageStr = ImageUsageToString(usages[i]);
                if(usages[i] != ImageUsage::Unknown)
                {
                    // baseName.image.index.ext -> baseName.image.index.usage.ext
                    auto pos = fileName.rfind(ext);
                    if(pos!=std::string::npos)
                        fileName.insert(pos, std::string(".") + usageStr);
                }
            }
            auto outPath = targetDir / fileName;

            if (!img.data.empty())
            {
                std::ofstream ofs(outPath, std::ios::binary);
                if (!ofs)
                {
                    std::cerr << "[Export] Failed to write image: " << outPath << "\n";
                    return false;
                }
                ofs.write(reinterpret_cast<const char *>(img.data.data()), static_cast<std::streamsize>(img.data.size()));
                ofs.close();
                std::cout << "[Export] Wrote embedded image: " << outPath << " (" << ImageUsageToString(usages[i]) << ")\n";
                continue;
            }
            if (!img.uri.empty())
            {
                std::filesystem::path srcDir = std::filesystem::path(model.gltf_source).parent_path();
                std::filesystem::path srcPath = srcDir / img.uri;
                if(!CopyImageFileWithUsage(srcPath, outPath, usages[i]))
                    return false;
                continue;
            }
            std::cout << "[Export] Skipped image index " << i << " (no data, no uri, usage=" << ImageUsageToString(usages[i]) << ").\n";
        }
        return true;
    }
}
