#include "PBRMaterial.h"
#include "Model.h"
#include <nlohmann/json.hpp>
#include <unordered_map>
#include "../export/ExportFileNames.h"
#include "../export/ImageMime.h"
#include "../export/SamplerStrings.h"

using nlohmann::json;

namespace pure
{
    static bool IsAll(const float *v, int n, float value)
    { for (int i=0;i<n;++i) if (v[i]!=value) return false; return true; }
    static bool IsZero3(const float *v){ return v[0]==0.f && v[1]==0.f && v[2]==0.f; }

    static json TextureRefToJson(const TextureRef &ref,
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

    static json PBRToJson(const PBRMetallicRoughness &p,
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

    nlohmann::json PBRMaterial::toJson(const Model &model,
                                       const std::unordered_map<std::size_t,int32_t> &texRemap,
                                       const std::unordered_map<std::size_t,int32_t> &imgRemap,
                                       const std::unordered_map<std::size_t,int32_t> &sampRemap,
                                       const std::string &baseName) const
    {
        json j = json::object();
        // build base
        j["name"] = name;
        j["type"] = type;

        // pbr
        if (auto p = PBRToJson(pbr, texRemap); !p.empty()) j["pbrMetallicRoughness"] = std::move(p);

        // normal/occlusion/emissive textures
        if (normalTexture && !TextureRefToJson(*normalTexture, texRemap).empty())
            j["normalTexture"] = TextureRefToJson(*normalTexture, texRemap);
        if (occlusionTexture && !TextureRefToJson(*occlusionTexture, texRemap).empty())
            j["occlusionTexture"] = TextureRefToJson(*occlusionTexture, texRemap);
        if (emissiveTexture && !TextureRefToJson(*emissiveTexture, texRemap).empty())
            j["emissiveTexture"] = TextureRefToJson(*emissiveTexture, texRemap);

        if (!IsZero3(emissiveFactor)) j["emissiveFactor"] = { emissiveFactor[0], emissiveFactor[1], emissiveFactor[2] };

        // alphaMode
        switch (alphaMode)
        {
            case AlphaMode::Opaque: break;
            case AlphaMode::Mask:
                j["alphaMode"] = "MASK";
                if (alphaCutoff != 0.5f) j["alphaCutoff"] = alphaCutoff;
                break;
            case AlphaMode::Blend:
                j["alphaMode"] = "BLEND"; break;
        }
        if (doubleSided) j["doubleSided"] = true;

        // textures/images/samplers remap
        if (!usedTextures.empty())
        {
            json arr = json::array();
            for (std::size_t gi : usedTextures)
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
        if (!usedImages.empty())
        {
            json arr = json::array();
            for (std::size_t gi : usedImages)
            {
                const auto &img = model.images[gi];
                json ji;
                if (!img.mimeType.empty()) ji["mimeType"] = img.mimeType;
                ji["file"] = exporters::MakeImageFileName(baseName, img.name, static_cast<int32_t>(gi), exporters::GuessImageExtension(img.mimeType));
                arr.push_back(std::move(ji));
            }
            j["images"] = std::move(arr);
        }
        if (!usedSamplers.empty())
        {
            json arr = json::array();
            for (std::size_t gi : usedSamplers)
            {
                const auto &s = model.samplers[gi];
                json js; js["wrapS"] = exporters::WrapModeToString(s.wrapS); js["wrapT"] = exporters::WrapModeToString(s.wrapT);
                if (s.magFilter) js["magFilter"] = exporters::FilterToString(*s.magFilter);
                if (s.minFilter) js["minFilter"] = exporters::FilterToString(*s.minFilter);
                arr.push_back(std::move(js));
            }
            j["samplers"] = std::move(arr);
        }

        return j;
    }
}
