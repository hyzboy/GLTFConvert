#include "ImageUsage.h"
#include "pure/Model.h"
#include "pure/PBRMaterial.h"
#include <algorithm>

namespace exporters
{
    const char *ImageUsageToString(ImageUsage u)
    {
        switch(u)
        {
            case ImageUsage::BaseColor: return "BaseColor";
            case ImageUsage::MetallicRoughness: return "MetallicRoughness";
            case ImageUsage::Normal: return "Normal";
            case ImageUsage::Occlusion: return "Occlusion";
            case ImageUsage::Emissive: return "Emissive";
            case ImageUsage::Specular: return "Specular";
            case ImageUsage::SpecularColor: return "SpecularColor";
            case ImageUsage::Clearcoat: return "Clearcoat";
            case ImageUsage::ClearcoatRoughness: return "ClearcoatRoughness";
            case ImageUsage::ClearcoatNormal: return "ClearcoatNormal";
            case ImageUsage::SheenColor: return "SheenColor";
            case ImageUsage::SheenRoughness: return "SheenRoughness";
            case ImageUsage::Transmission: return "Transmission";
            case ImageUsage::DiffuseTransmission: return "DiffuseTransmission";
            case ImageUsage::Thickness: return "Thickness";
            case ImageUsage::Anisotropy: return "Anisotropy";
            case ImageUsage::Iridescence: return "Iridescence";
            case ImageUsage::IridescenceThickness: return "IridescenceThickness";
            default: return "Unknown";
        }
    }

    struct ScoreEntry { int weight=0; ImageUsage usage=ImageUsage::Unknown; };

    static void AddScore(std::vector<ScoreEntry> &scores, std::size_t img, ImageUsage usage, int w=1)
    {
        if(img>=scores.size()) return; // safety
        // accumulate weight per usage: prefer first strong usage but allow override by higher weight
        // we simply choose greatest weight at the end; if tie keep earlier (stable)
        // So we just store max weight if larger
        if(w > scores[img].weight) { scores[img].weight = w; scores[img].usage = usage; }
    }

    void BuildImageUsage(const pure::Model &model, std::vector<ImageUsage> &outUsages)
    {
        outUsages.assign(model.images.size(), ImageUsage::Unknown);
        if(model.images.empty()) return;
        std::vector<ScoreEntry> scores(model.images.size());

        auto recordTextureRef=[&](const pure::TextureRef &ref, ImageUsage usage, int w=1)
        {
            if(!ref.index) return; // texture index
            std::size_t texIdx=*ref.index;
            if(texIdx>=model.textures.size()) return;
            const auto &tex=model.textures[texIdx];
            if(!tex.image) return;
            std::size_t imgIdx = tex.image;
            if(imgIdx>=model.images.size()) return;
            AddScore(scores,imgIdx,usage,w);
        };

        for(const auto &matPtr : model.materials)
        {
            const pure::PBRMaterial *mat = dynamic_cast<const pure::PBRMaterial*>(matPtr.get());
            if (!mat) continue;
            recordTextureRef(mat->pbr.baseColorTexture, ImageUsage::BaseColor, 10);
            recordTextureRef(mat->pbr.metallicRoughnessTexture, ImageUsage::MetallicRoughness, 9);
            if (mat->normalTexture) recordTextureRef(*mat->normalTexture, ImageUsage::Normal, 10);
            if (mat->occlusionTexture) recordTextureRef(*mat->occlusionTexture, ImageUsage::Occlusion, 6);
            if (mat->emissiveTexture) recordTextureRef(*mat->emissiveTexture, ImageUsage::Emissive, 6);
            // No extensions in PBRMaterial
        }

        for(std::size_t i=0;i<scores.size();++i)
            outUsages[i]=scores[i].usage;
    }

    ImageUsage GuessImageUsage(const pure::Model &model, std::size_t imageIndex)
    {
        if(imageIndex>=model.images.size()) return ImageUsage::Unknown;
        std::vector<ImageUsage> usages; BuildImageUsage(model, usages);
        return usages[imageIndex];
    }
}
