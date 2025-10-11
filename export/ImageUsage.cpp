#include "ImageUsage.h"
#include "pure/Model.h"
#include "pure/Material.h"
#include "gltf/GLTFMaterialImpl.h"
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

        auto recordTextureRef=[&](const GLTFTextureRef &ref, ImageUsage usage, int w=1)
        {
            if(!ref.index) return; // texture index
            std::size_t texIdx=*ref.index;
            if(texIdx>=model.textures.size()) return;
            const auto &tex=model.textures[texIdx];
            if(!tex.image) return;
            std::size_t imgIdx=*tex.image;
            if(imgIdx>=model.images.size()) return;
            AddScore(scores,imgIdx,usage,w);
        };

        for(const auto &matPtr : model.materials)
        {
            const gltf::GLTFMaterialImpl *mat = dynamic_cast<const gltf::GLTFMaterialImpl*>(matPtr.get());
            if (!mat) continue;
            const auto& gm = mat->gltfMaterial;
            recordTextureRef(gm.pbr.baseColorTexture, ImageUsage::BaseColor, 10);
            recordTextureRef(gm.pbr.metallicRoughnessTexture, ImageUsage::MetallicRoughness, 9);
            recordTextureRef(gm.normalTexture, ImageUsage::Normal, 10);
            recordTextureRef(gm.occlusionTexture, ImageUsage::Occlusion, 6);
            recordTextureRef(gm.emissiveTexture, ImageUsage::Emissive, 6);
            if(gm.extSpecular)
            {
                recordTextureRef(gm.extSpecular->specularTexture, ImageUsage::Specular, 5);
                recordTextureRef(gm.extSpecular->specularColorTexture, ImageUsage::SpecularColor, 5);
            }
            if(gm.extClearcoat)
            {
                recordTextureRef(gm.extClearcoat->clearcoatTexture, ImageUsage::Clearcoat, 4);
                recordTextureRef(gm.extClearcoat->clearcoatRoughnessTexture, ImageUsage::ClearcoatRoughness, 4);
                recordTextureRef(gm.extClearcoat->clearcoatNormalTexture, ImageUsage::ClearcoatNormal, 8);
            }
            if(gm.extSheen)
            {
                recordTextureRef(gm.extSheen->sheenColorTexture, ImageUsage::SheenColor, 4);
                recordTextureRef(gm.extSheen->sheenRoughnessTexture, ImageUsage::SheenRoughness, 4);
            }
            if(gm.extTransmission)
            {
                recordTextureRef(gm.extTransmission->transmissionTexture, ImageUsage::Transmission, 4);
            }
            if(gm.extDiffuseTransmission)
            {
                recordTextureRef(gm.extDiffuseTransmission->diffuseTransmissionTexture, ImageUsage::DiffuseTransmission, 4);
            }
            if(gm.extVolume)
            {
                recordTextureRef(gm.extVolume->thicknessTexture, ImageUsage::Thickness, 3);
            }
            if(gm.extAnisotropy)
            {
                recordTextureRef(gm.extAnisotropy->texture, ImageUsage::Anisotropy, 3);
            }
            if(gm.extIridescence)
            {
                recordTextureRef(gm.extIridescence->texture, ImageUsage::Iridescence, 3);
                recordTextureRef(gm.extIridescence->thicknessTexture, ImageUsage::IridescenceThickness, 3);
            }
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
