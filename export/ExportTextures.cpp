#include "ExportTextures.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_set>
#include "ExportFileNames.h"

namespace exporters
{
    namespace
    {
        static std::string GuessImageExtension(const std::string &mime)
        {
            if (mime == "image/png")  return ".png";
            if (mime == "image/jpeg") return ".jpg";
            if (mime == "image/ktx2") return ".ktx2";
            if (mime == "image/vnd-ms.dds") return ".dds";
            if (mime == "image/webp")  return ".webp";
            return ".bin";
        }
        static std::string WrapModeToString(int v)
        {
            switch (v)
            {
                case 33071: return "CLAMP_TO_EDGE";
                case 33648: return "MIRRORED_REPEAT";
                case 10497: return "REPEAT";
                default:    return std::to_string(v);
            }
        }
        static std::string FilterToString(int v)
        {
            switch (v)
            {
                case 9728: return "NEAREST";
                case 9729: return "LINEAR";
                case 9984: return "NEAREST_MIPMAP_NEAREST";
                case 9985: return "LINEAR_MIPMAP_NEAREST";
                case 9986: return "NEAREST_MIPMAP_LINEAR";
                case 9987: return "LINEAR_MIPMAP_LINEAR";
                default:   return std::to_string(v);
            }
        }
    }

    void ExportTexturesJson(const std::string &gltfSource,
                            const std::vector<pure::Image> &allImages,
                            const std::vector<pure::Texture> &allTextures,
                            const std::vector<pure::Sampler> &allSamplers,
                            const std::vector<std::size_t> *usedImages,
                            const std::vector<std::size_t> *usedTextures,
                            const std::vector<std::size_t> *usedSamplers,
                            const std::filesystem::path &targetDir,
                            const std::string &sceneName)
    {
        std::unordered_set<std::size_t> imgSet, texSet, sampSet;
        if (usedImages)   imgSet.insert(usedImages->begin(),   usedImages->end());
        if (usedTextures) texSet.insert(usedTextures->begin(), usedTextures->end());
        if (usedSamplers) sampSet.insert(usedSamplers->begin(), usedSamplers->end());

        if (allImages.empty() && allTextures.empty() && allSamplers.empty()) return;
        if (usedImages && imgSet.empty() && usedTextures && texSet.empty() && usedSamplers && sampSet.empty()) return;

        nlohmann::json root;
        const std::string baseName = std::filesystem::path(gltfSource).stem().string();

        // Images
        if (!allImages.empty())
        {
            nlohmann::json arr = nlohmann::json::array();
            for (std::size_t i = 0; i < allImages.size(); ++i)
            {
                if (usedImages && !imgSet.count(i)) continue;
                const auto &img = allImages[i];
                nlohmann::json ji;
                ji["index"] = i;
                ji["name"]  = !img.name.empty() ? img.name : ("image." + std::to_string(i));
                std::string fileName = MakeImageFileName(baseName, img.name, static_cast<int32_t>(i), GuessImageExtension(img.mimeType));
                ji["file"] = fileName;
                if (!img.mimeType.empty()) ji["mimeType"] = img.mimeType;
                arr.push_back(std::move(ji));
            }
            if (!arr.empty()) root["images"] = std::move(arr);
        }

        // Textures
        if (!allTextures.empty())
        {
            nlohmann::json arr = nlohmann::json::array();
            for (std::size_t i = 0; i < allTextures.size(); ++i)
            {
                if (usedTextures && !texSet.count(i)) continue;
                const auto &t = allTextures[i];
                nlohmann::json jt;
                jt["index"] = i;
                if (!t.name.empty()) jt["name"] = t.name;
                if (t.image)   jt["image"]   = static_cast<int>(*t.image);
                if (t.sampler) jt["sampler"] = static_cast<int>(*t.sampler);
                arr.push_back(std::move(jt));
            }
            if (!arr.empty()) root["textures"] = std::move(arr);
        }

        // Samplers
        if (!allSamplers.empty())
        {
            nlohmann::json arr = nlohmann::json::array();
            for (std::size_t i = 0; i < allSamplers.size(); ++i)
            {
                if (usedSamplers && !sampSet.count(i)) continue;
                const auto &s = allSamplers[i];
                nlohmann::json js;
                js["index"] = i;
                js["wrapS"] = WrapModeToString(s.wrapS);
                js["wrapT"] = WrapModeToString(s.wrapT);
                if (s.magFilter) js["magFilter"] = FilterToString(*s.magFilter);
                if (s.minFilter) js["minFilter"] = FilterToString(*s.minFilter);
                if (!s.name.empty()) js["name"] = s.name;
                arr.push_back(std::move(js));
            }
            if (!arr.empty()) root["samplers"] = std::move(arr);
        }

        if (root.empty()) return;

        std::string texturesFile = MakeTexturesJsonFileName(baseName, sceneName);
        auto outPath = targetDir / texturesFile;
        std::ofstream ofs(outPath, std::ios::binary);
        if (!ofs)
        {
            std::cerr << "[Export] Failed to write textures file: " << outPath << "\n";
            return;
        }
        ofs << root.dump(4);
        std::cout << "[Export] Wrote textures file: " << outPath << "\n";
    }
}
