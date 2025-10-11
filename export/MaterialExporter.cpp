#include "MaterialExporter.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <vector>
#include <filesystem>
#include <unordered_map>
#include <limits>

#include "pure/Material.h"
#include "pure/PBRMaterial.h"
#include "pure/Model.h"
#include "pure/Texture.h"
#include "pure/Image.h"
#include "pure/Sampler.h"
#include "ExportFileNames.h"
#include "ImageMime.h" // added
#include "SamplerStrings.h" // added

using nlohmann::json;

namespace exporters
{
    // Removed duplicated GuessImageExtension (now in ImageMime.cpp)

    // Helpers -----------------------------------------------------------------

    static bool IsAll(const float *v, int n, float value)
    { for (int i=0;i<n;++i) if (v[i]!=value) return false; return true; }

    static bool IsZero3(const float *v){ return v[0]==0.f && v[1]==0.f && v[2]==0.f; }

    static json TextureRefToJson(const pure::TextureRef &ref,
                                 const std::unordered_map<std::size_t,int32_t> &texRemap)
    {
        if (!ref.index) return json();
        json j = json::object();
        auto it = texRemap.find(*ref.index);
        if (it != texRemap.end()) j["texture"] = it->second;
        if (ref.texCoord != 0) j["texCoord"] = ref.texCoord;
        if (ref.scale    != 1.0f) j["scale"]    = ref.scale;
        if (ref.strength != 1.0f) j["strength"] = ref.strength;
        return j;
    }

    static json PBRToJson(const pure::PBRMetallicRoughness &p,
                          const std::unordered_map<std::size_t,int32_t> &texRemap)
    {
        json j = json::object();
        if (!IsAll(p.baseColorFactor,4,1.f))
            j["baseColorFactor"] = { p.baseColorFactor[0], p.baseColorFactor[1], p.baseColorFactor[2], p.baseColorFactor[3] };
        if (p.metallicFactor  != 1.0f) j["metallicFactor"]  = p.metallicFactor;
        if (p.roughnessFactor != 1.0f) j["roughnessFactor"] = p.roughnessFactor;
        if (auto bc = TextureRefToJson(p.baseColorTexture, texRemap); !bc.empty())
            j["baseColorTexture"] = std::move(bc);
        if (auto mr = TextureRefToJson(p.metallicRoughnessTexture, texRemap); !mr.empty())
            j["metallicRoughnessTexture"] = std::move(mr);
        return j; // may be empty – caller decides whether to include
    }

    static json MaterialToJson(const pure::PBRMaterial &m, const pure::Model &model,
                               const std::unordered_map<std::size_t,int32_t> &texRemap,
                               const std::unordered_map<std::size_t,int32_t> &imgRemap,
                               const std::unordered_map<std::size_t,int32_t> &sampRemap,
                               const std::string &baseName)
    {
        json j = m.toJson();

        // alphaMode (only if not OPAQUE)
        switch (m.alphaMode)
        {
            case pure::AlphaMode::Opaque: break;
            case pure::AlphaMode::Mask:
                j["alphaMode"] = "MASK";
                if (m.alphaCutoff != 0.5f) j["alphaCutoff"] = m.alphaCutoff; // cutoff only meaningful here
                break;
            case pure::AlphaMode::Blend:
                j["alphaMode"] = "BLEND"; break;
        }
        if (m.doubleSided) j["doubleSided"] = true;

        // Keep resource remap lists (unchanged) ---------------------------------
        if (!m.usedTextures.empty())
        {
            json arr = json::array();
            for (std::size_t gi : m.usedTextures)
            {
                const auto &t = model.textures[gi];
                json jt;
                if (t.image)
                {
                    auto itImg = imgRemap.find(t.image);
                    if (itImg != imgRemap.end()) jt["image"] = itImg->second;
                }
                if (t.sampler)
                {
                    auto itS = sampRemap.find(*t.sampler);
                    if (itS != sampRemap.end()) jt["sampler"] = itS->second;
                }
                arr.push_back(std::move(jt));
            }
            j["textures"] = std::move(arr);
        }
        if (!m.usedImages.empty())
        {
            json arr = json::array();
            for (std::size_t gi : m.usedImages)
            {
                const auto &img = model.images[gi];
                json ji;
                if (!img.mimeType.empty()) ji["mimeType"] = img.mimeType;
                ji["file"] = MakeImageFileName(baseName, img.name, static_cast<int32_t>(gi), GuessImageExtension(img.mimeType));
                arr.push_back(std::move(ji));
            }
            j["images"] = std::move(arr);
        }
        if (!m.usedSamplers.empty())
        {
            json arr = json::array();
            for (std::size_t gi : m.usedSamplers)
            {
                const auto &s = model.samplers[gi];
                json js; js["wrapS"] = WrapModeToString(s.wrapS); js["wrapT"] = WrapModeToString(s.wrapT);
                if (s.magFilter) js["magFilter"] = FilterToString(*s.magFilter);
                if (s.minFilter) js["minFilter"] = FilterToString(*s.minFilter);
                arr.push_back(std::move(js));
            }
            j["samplers"] = std::move(arr);
        }

        return j;
    }

    bool ExportMaterials(const pure::Model &model, const std::filesystem::path &dir)
    {
        if (model.materials.empty()) return true;

        std::error_code ec; std::filesystem::create_directories(dir, ec);
        std::string baseName = dir.filename().string();
        if (auto pos = baseName.find(".StaticMesh"); pos != std::string::npos) baseName = baseName.substr(0, pos);

        for (std::size_t i = 0; i < model.materials.size(); ++i)
        {
            const auto &matPtr = model.materials[i];
            const pure::PBRMaterial *m = dynamic_cast<const pure::PBRMaterial*>(matPtr.get());
            if (!m)
            {
                std::cerr << "[Export] Unsupported material type at index " << i << "\n";
                continue;
            }

            std::unordered_map<std::size_t,int32_t> texRemap, imgRemap, sampRemap;
            for (std::size_t ti = 0; ti < m->usedTextures.size(); ++ti) texRemap[m->usedTextures[ti]] = static_cast<int32_t>(ti);
            for (std::size_t ii = 0; ii < m->usedImages.size();   ++ii) imgRemap[m->usedImages[ii]]   = static_cast<int32_t>(ii);
            for (std::size_t si = 0; si < m->usedSamplers.size(); ++si) sampRemap[m->usedSamplers[si]] = static_cast<int32_t>(si);

            auto filePath = dir / MakeMaterialFileName(baseName, m->name, static_cast<int32_t>(i));
            std::ofstream ofs(filePath, std::ios::binary);
            if (!ofs)
            {
                std::cerr << "[Export] Cannot write material: " << filePath << "\n";
                return false;
            }
            ofs << MaterialToJson(*m, model, texRemap, imgRemap, sampRemap, baseName).dump(4);
            ofs.close();
            std::cout << "[Export] Saved material: " << filePath << "\n";
        }
        return true;
    }
}
