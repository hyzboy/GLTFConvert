#include "ExportImages.h"
#include "pure/Model.h"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace exporters {
    namespace {
        static std::string GuessExtension(const std::string &mime) {
            if (mime == "image/png") return ".png";
            if (mime == "image/jpeg") return ".jpg";
            if (mime == "image/ktx2") return ".ktx2";
            if (mime == "image/vnd-ms.dds") return ".dds";
            if (mime == "image/webp") return ".webp";
            return ".bin"; // fallback
        }
    }

    bool ExportImages(const pure::Model &model, const std::filesystem::path &targetDir) {
        if (model.images.empty()) return true;
        const std::string baseName = std::filesystem::path(model.gltf_source).stem().string();

        for (std::size_t i = 0; i < model.images.size(); ++i) {
            const auto &img = model.images[i];
            std::string ext = GuessExtension(img.mimeType);
            std::string fileBase;
            if (!img.name.empty()) fileBase = baseName + "." + img.name; else fileBase = baseName + ".image." + std::to_string(i);

            auto outPath = targetDir / (fileBase + ext);

            if (!img.data.empty()) {
                std::ofstream ofs(outPath, std::ios::binary);
                if (!ofs) { std::cerr << "[Export] Failed to write image: " << outPath << "\n"; return false; }
                ofs.write(reinterpret_cast<const char*>(img.data.data()), static_cast<std::streamsize>(img.data.size()));
                ofs.close();
                std::cout << "[Export] Wrote embedded image: " << outPath << "\n";
            } else if (!img.uri.empty()) {
                // copy external file referenced by URI relative to original gltf directory
                std::filesystem::path srcDir = std::filesystem::path(model.gltf_source).parent_path();
                std::filesystem::path srcPath = srcDir / img.uri;
                std::error_code ec; std::filesystem::copy_file(srcPath, outPath, std::filesystem::copy_options::overwrite_existing, ec);
                if (ec) { std::cerr << "[Export] Copy image failed: " << srcPath << " -> " << outPath << " : " << ec.message() << "\n"; return false; }
                std::cout << "[Export] Copied external image: " << outPath << "\n";
            } else {
                // no data; possibly bufferView referenced (need decoding elsewhere)
                std::cout << "[Export] Skipped image index " << i << " (no data, no uri).\n";
            }
        }
        return true;
    }
}
