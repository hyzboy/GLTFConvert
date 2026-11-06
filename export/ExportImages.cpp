#include "ExportImages.h"
#include "pure/Model.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_set>
#include <cstdlib>
#include "ExportFileNames.h"
#include "NativeMaterialExport.h" // shared image naming with recipe writer
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
        texconv::ConvertOptions opts;
        if(usage == ImageUsage::Normal || usage == ImageUsage::ClearcoatNormal)
            opts.normal_map = true;

        // Try TexConv convert first
        if(texconv::Convert(srcPath, dstPath, opts))
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
            const std::string fileName = MakeExportedImageFileName(model, i, baseName, usages);
            auto outPath = targetDir / fileName;

            if (img.data && !img.data->empty())
            {
                // embedded image data
                std::ofstream ofs(outPath, std::ios::binary);
                if (!ofs)
                {
                    std::cerr << "[Export] Failed to write image: " << outPath << "\n";
                    return false;
                }
                ofs.write(reinterpret_cast<const char *>(img.data->data()), static_cast<std::streamsize>(img.data->size()));
                ofs.close();
                std::cout << "[Export] Wrote embedded image: " << outPath << " (" << ImageUsageToString(usages[i]) << ")\n";
                // 内嵌贴图也走 TexConv 直出 .Tex2D（配方引用的是 .Tex2D 资产名）
                texconv::ConvertOptions opts;
                if(usages[i] == ImageUsage::Normal || usages[i] == ImageUsage::ClearcoatNormal)
                    opts.normal_map = true;
                if (texconv::Convert(outPath, outPath, opts))
                {
                    // 与外部 URI 分支口径一致：成功转换后不留中间原图
                    std::error_code tec;
                    std::filesystem::remove(outPath, tec);
                }
                else
                    std::cout << "[Export] TexConv unavailable/failed; embedded image kept as-is (run TexConv manually).\n";
                continue;
            }
            else if (img.uri && !img.uri->empty())
            {
                // external image URI - copy from source directory
                std::filesystem::path srcDir = std::filesystem::path(model.model_source).parent_path();
                std::filesystem::path srcPath = srcDir / *img.uri;
                if(!CopyImageFileWithUsage(srcPath, outPath, usages[i]))
                    return false;
                continue;
            }
            std::cout << "[Export] Skipped image index " << i << " (no data, no uri, usage=" << ImageUsageToString(usages[i]) << ").\n";
        }
        return true;
    }
}
