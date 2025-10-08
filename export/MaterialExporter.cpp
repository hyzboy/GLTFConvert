#include "MaterialExporter.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <vector>
#include <filesystem>
#include <unordered_map>

#include "pure/Material.h"
#include "pure/Model.h"
#include "pure/Texture.h"
#include "pure/Image.h"
#include "pure/Sampler.h"
#include "ExportFileNames.h"

using nlohmann::json;

namespace exporters
{
    // Guess file extension from mime type (duplicated minimal logic to avoid dependency cycle)
    static std::string GuessImageExtension(const std::string &mime)
    {
        if (mime == "image/png")  return ".png";
        if (mime == "image/jpeg") return ".jpg";
        if (mime == "image/ktx2") return ".ktx2";
        if (mime == "image/vnd-ms.dds") return ".dds";
        if (mime == "image/webp") return ".webp";
        return ".bin"; // fallback
    }

    static std::string WrapModeToString(int v)
    {
        switch (v)
        {
            case 33071: return "CLAMP_TO_EDGE";      // GL_CLAMP_TO_EDGE
            case 33648: return "MIRRORED_REPEAT";   // GL_MIRRORED_REPEAT
            case 10497: return "REPEAT";            // GL_REPEAT
            default:    return std::to_string(v);
        }
    }
    static std::string FilterToString(int v)
    {
        switch (v)
        {
            case 9728: return "NEAREST";                 // GL_NEAREST
            case 9729: return "LINEAR";                  // GL_LINEAR
            case 9984: return "NEAREST_MIPMAP_NEAREST";  // GL_NEAREST_MIPMAP_NEAREST
            case 9985: return "LINEAR_MIPMAP_NEAREST";   // GL_LINEAR_MIPMAP_NEAREST
            case 9986: return "NEAREST_MIPMAP_LINEAR";   // GL_NEAREST_MIPMAP_LINEAR
            case 9987: return "LINEAR_MIPMAP_LINEAR";    // GL_LINEAR_MIPMAP_LINEAR
            default:   return std::to_string(v);
        }
    }

    // Convert a GLTFTextureRef to local remapped texture index using provided mapping
    static json TextureRefToJson(const GLTFTextureRef &ref,
                                 const std::unordered_map<std::size_t,int32_t> &texRemap)
    {
        json j = json::object();
        if (ref.index)
        {
            auto it = texRemap.find(*ref.index);
            if (it != texRemap.end())
                j["texture"] = it->second;
        }
        j["texCoord"] = ref.texCoord;
        if (ref.scale    != 1.0f) j["scale"]    = ref.scale;
        if (ref.strength != 1.0f) j["strength"] = ref.strength;
        return j;
    }

    static json PBRToJson(const GLTFPBRMetallicRoughness &p,
                          const std::unordered_map<std::size_t,int32_t> &texRemap)
    {
        json j;
        j["baseColorFactor"] = { p.baseColorFactor[0], p.baseColorFactor[1], p.baseColorFactor[2], p.baseColorFactor[3] };
        j["metallicFactor"]  = p.metallicFactor;
        j["roughnessFactor"] = p.roughnessFactor;
        j["baseColorTexture"]        = TextureRefToJson(p.baseColorTexture, texRemap);
        j["metallicRoughnessTexture"] = TextureRefToJson(p.metallicRoughnessTexture, texRemap);
        return j;
    }

    static json ExtensionsToJson(const pure::Material &m,
                                 const std::unordered_map<std::size_t,int32_t> &texRemap)
    {
        json ext = json::object();
        if (m.extEmissiveStrength)
            ext["KHR_materials_emissive_strength"] = { {"emissiveStrength", m.extEmissiveStrength->emissiveStrength } };
        if (m.extUnlit)
            ext["KHR_materials_unlit"] = json::object();
        if (m.extIOR)
            ext["KHR_materials_ior"] = { {"ior", m.extIOR->ior } };
        if (m.extTransmission)
        {
            json tx; tx["transmissionFactor"] = m.extTransmission->transmissionFactor;
            tx["transmissionTexture"] = TextureRefToJson(m.extTransmission->transmissionTexture, texRemap);
            ext["KHR_materials_transmission"] = std::move(tx);
        }
        if (m.extDiffuseTransmission)
        {
            json dx; dx["diffuseTransmissionFactor"]  = m.extDiffuseTransmission->diffuseTransmissionFactor;
            dx["diffuseTransmissionTexture"] = TextureRefToJson(m.extDiffuseTransmission->diffuseTransmissionTexture, texRemap);
            ext["KHR_materials_diffuse_transmission"] = std::move(dx);
        }
        if (m.extSpecular)
        {
            json sx;
            sx["specularFactor"]      = m.extSpecular->specularFactor;
            sx["specularColorFactor"] = { m.extSpecular->specularColorFactor[0], m.extSpecular->specularColorFactor[1], m.extSpecular->specularColorFactor[2] };
            sx["specularTexture"]      = TextureRefToJson(m.extSpecular->specularTexture, texRemap);
            sx["specularColorTexture"] = TextureRefToJson(m.extSpecular->specularColorTexture, texRemap);
            ext["KHR_materials_specular"] = std::move(sx);
        }
        if (m.extClearcoat)
        {
            json cx;
            cx["clearcoatFactor"]          = m.extClearcoat->clearcoatFactor;
            cx["clearcoatRoughnessFactor"] = m.extClearcoat->clearcoatRoughnessFactor;
            cx["clearcoatTexture"]         = TextureRefToJson(m.extClearcoat->clearcoatTexture, texRemap);
            cx["clearcoatRoughnessTexture"] = TextureRefToJson(m.extClearcoat->clearcoatRoughnessTexture, texRemap);
            cx["clearcoatNormalTexture"]   = TextureRefToJson(m.extClearcoat->clearcoatNormalTexture, texRemap);
            ext["KHR_materials_clearcoat"] = std::move(cx);
        }
        if (m.extSheen)
        {
            json sh;
            sh["sheenColorFactor"]    = { m.extSheen->sheenColorFactor[0], m.extSheen->sheenColorFactor[1], m.extSheen->sheenColorFactor[2] };
            sh["sheenRoughnessFactor"] = m.extSheen->sheenRoughnessFactor;
            sh["sheenColorTexture"]    = TextureRefToJson(m.extSheen->sheenColorTexture, texRemap);
            sh["sheenRoughnessTexture"] = TextureRefToJson(m.extSheen->sheenRoughnessTexture, texRemap);
            ext["KHR_materials_sheen"] = std::move(sh);
        }
        if (m.extVolume)
        {
            json vo;
            vo["thicknessFactor"]  = m.extVolume->thicknessFactor;
            vo["thicknessTexture"] = TextureRefToJson(m.extVolume->thicknessTexture, texRemap);
            vo["attenuationColor"] = { m.extVolume->attenuationColor[0], m.extVolume->attenuationColor[1], m.extVolume->attenuationColor[2] };
            vo["attenuationDistance"] = m.extVolume->attenuationDistance;
            ext["KHR_materials_volume"] = std::move(vo);
        }
        if (m.extAnisotropy)
        {
            json an;
            an["anisotropyStrength"] = m.extAnisotropy->strength;
            an["anisotropyRotation"] = m.extAnisotropy->rotation;
            an["anisotropyTexture"]  = TextureRefToJson(m.extAnisotropy->texture, texRemap);
            ext["KHR_materials_anisotropy"] = std::move(an);
        }
        if (m.extIridescence)
        {
            json ir;
            ir["iridescenceFactor"]           = m.extIridescence->factor;
            ir["iridescenceIor"]              = m.extIridescence->ior;
            ir["iridescenceThicknessMinimum"] = m.extIridescence->thicknessMinimum;
            ir["iridescenceThicknessMaximum"] = m.extIridescence->thicknessMaximum;
            ir["iridescenceTexture"]          = TextureRefToJson(m.extIridescence->texture, texRemap);
            ir["iridescenceThicknessTexture"] = TextureRefToJson(m.extIridescence->thicknessTexture, texRemap);
            ext["KHR_materials_iridescence"] = std::move(ir);
        }
        return ext;
    }

    static json MaterialToJson(const pure::Material &m, const pure::Model &model,
                               const std::unordered_map<std::size_t,int32_t> &texRemap,
                               const std::unordered_map<std::size_t,int32_t> &imgRemap,
                               const std::unordered_map<std::size_t,int32_t> &sampRemap,
                               const std::string &baseName)
    {
        json j;
        if (!m.name.empty()) j["name"] = m.name;
        j["pbrMetallicRoughness"] = PBRToJson(m.pbr, texRemap);
        j["normalTexture"]        = TextureRefToJson(m.normalTexture, texRemap);
        j["occlusionTexture"]     = TextureRefToJson(m.occlusionTexture, texRemap);
        j["emissiveTexture"]      = TextureRefToJson(m.emissiveTexture, texRemap);
        j["emissiveFactor"]       = { m.emissiveFactor[0], m.emissiveFactor[1], m.emissiveFactor[2] };

        const char* alphaStr = "OPAQUE";
        switch (m.alphaMode)
        {
            case pure::Material::AlphaMode::Opaque: alphaStr = "OPAQUE"; break;
            case pure::Material::AlphaMode::Mask:   alphaStr = "MASK";   break;
            case pure::Material::AlphaMode::Blend:  alphaStr = "BLEND";  break;
        }
        j["alphaMode"]   = alphaStr;
        j["alphaCutoff"] = m.alphaCutoff;
        j["doubleSided"] = m.doubleSided;

        json ext = ExtensionsToJson(m, texRemap);
        if (!ext.empty()) j["extensions"] = std::move(ext);

        if (!m.usedTextures.empty())
        {
            json arr = json::array();
            for (std::size_t gi : m.usedTextures)
            {
                const auto &t = model.textures[gi];
                json jt; // no original name
                if (t.image)
                {
                    auto itImg = imgRemap.find(*t.image);
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
            const auto &m = model.materials[i];

            std::unordered_map<std::size_t,int32_t> texRemap, imgRemap, sampRemap;
            for (std::size_t ti = 0; ti < m.usedTextures.size(); ++ti) texRemap[m.usedTextures[ti]] = static_cast<int32_t>(ti);
            for (std::size_t ii = 0; ii < m.usedImages.size();   ++ii) imgRemap[m.usedImages[ii]]   = static_cast<int32_t>(ii);
            for (std::size_t si = 0; si < m.usedSamplers.size(); ++si) sampRemap[m.usedSamplers[si]] = static_cast<int32_t>(si);

            auto filePath = dir / MakeMaterialFileName(baseName, m.name, static_cast<int32_t>(i));
            std::ofstream ofs(filePath, std::ios::binary);
            if (!ofs)
            {
                std::cerr << "[Export] Cannot write material: " << filePath << "\n";
                return false;
            }
            ofs << MaterialToJson(m, model, texRemap, imgRemap, sampRemap, baseName).dump(4);
            ofs.close();
            std::cout << "[Export] Saved material: " << filePath << "\n";
        }
        return true;
    }
}
