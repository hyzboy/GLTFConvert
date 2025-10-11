#include "MaterialExporter.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <vector>
#include <filesystem>
#include <unordered_map>
#include <limits>

#include "pure/Material.h"
#include "pure/Model.h"
#include "pure/Texture.h"
#include "pure/Image.h"
#include "pure/Sampler.h"
#include "gltf/GLTFMaterialImpl.h"
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

    // Extensions filtering -----------------------------------------------------

    static json ExtensionsToJson(const gltf::GLTFMaterialImpl &m,
                                 const std::unordered_map<std::size_t,int32_t> &texRemap)
    {
        json ext = json::object();
        // KHR_materials_emissive_strength (default emissiveStrength = 1.0)
        if (m.extEmissiveStrength && m.extEmissiveStrength->emissiveStrength != 1.0f)
            ext["KHR_materials_emissive_strength"] = { {"emissiveStrength", m.extEmissiveStrength->emissiveStrength } };
        // KHR_materials_unlit has no properties (presence is the flag)
        if (m.extUnlit)
            ext["KHR_materials_unlit"] = json::object();
        // KHR_materials_ior (default ior = 1.5)
        if (m.extIOR && m.extIOR->ior != 1.5f)
            ext["KHR_materials_ior"] = { {"ior", m.extIOR->ior } };
        // KHR_materials_transmission (factor default 0)
        if (m.extTransmission)
        {
            bool hasTex = m.extTransmission->transmissionTexture.index.has_value();
            if (m.extTransmission->transmissionFactor != 0.f || hasTex)
            {
                json tx; if (m.extTransmission->transmissionFactor != 0.f)
                    tx["transmissionFactor"] = m.extTransmission->transmissionFactor;
                if (auto t = TextureRefToJson(pure::TextureRef{m.extTransmission->transmissionTexture.index, m.extTransmission->transmissionTexture.texCoord, m.extTransmission->transmissionTexture.scale, m.extTransmission->transmissionTexture.strength}, texRemap); !t.empty())
                    tx["transmissionTexture"] = std::move(t);
                ext["KHR_materials_transmission"] = std::move(tx);
            }
        }
        // Diffuse transmission (factor default 0)
        if (m.extDiffuseTransmission)
        {
            bool hasTex = m.extDiffuseTransmission->diffuseTransmissionTexture.index.has_value();
            if (m.extDiffuseTransmission->diffuseTransmissionFactor != 0.f || hasTex)
            {
                json dx; if (m.extDiffuseTransmission->diffuseTransmissionFactor != 0.f)
                    dx["diffuseTransmissionFactor"]  = m.extDiffuseTransmission->diffuseTransmissionFactor;
                if (auto t = TextureRefToJson(pure::TextureRef{m.extDiffuseTransmission->diffuseTransmissionTexture.index, m.extDiffuseTransmission->diffuseTransmissionTexture.texCoord, m.extDiffuseTransmission->diffuseTransmissionTexture.scale, m.extDiffuseTransmission->diffuseTransmissionTexture.strength}, texRemap); !t.empty())
                    dx["diffuseTransmissionTexture"] = std::move(t);
                ext["KHR_materials_diffuse_transmission"] = std::move(dx);
            }
        }
        // Specular (defaults: specularFactor=1, specularColorFactor=[1,1,1])
        if (m.extSpecular)
        {
            bool nonDefaultFactor = m.extSpecular->specularFactor != 1.f;
            bool nonDefaultColor = !(m.extSpecular->specularColorFactor[0]==1.f && m.extSpecular->specularColorFactor[1]==1.f && m.extSpecular->specularColorFactor[2]==1.f);
            bool hasTex = m.extSpecular->specularTexture.index.has_value() || m.extSpecular->specularColorTexture.index.has_value();
            if (nonDefaultFactor || nonDefaultColor || hasTex)
            {
                json sx; if (nonDefaultFactor) sx["specularFactor"] = m.extSpecular->specularFactor;
                if (nonDefaultColor) sx["specularColorFactor"] = { m.extSpecular->specularColorFactor[0], m.extSpecular->specularColorFactor[1], m.extSpecular->specularColorFactor[2] };
                if (auto t = TextureRefToJson(pure::TextureRef{m.extSpecular->specularTexture.index, m.extSpecular->specularTexture.texCoord, m.extSpecular->specularTexture.scale, m.extSpecular->specularTexture.strength}, texRemap); !t.empty())
                    sx["specularTexture"] = std::move(t);
                if (auto t = TextureRefToJson(pure::TextureRef{m.extSpecular->specularColorTexture.index, m.extSpecular->specularColorTexture.texCoord, m.extSpecular->specularColorTexture.scale, m.extSpecular->specularColorTexture.strength}, texRemap); !t.empty())
                    sx["specularColorTexture"] = std::move(t);
                ext["KHR_materials_specular"] = std::move(sx);
            }
        }
        // Clearcoat (defaults factors 0)
        if (m.extClearcoat)
        {
            bool nonDefault = (m.extClearcoat->clearcoatFactor != 0.f) || (m.extClearcoat->clearcoatRoughnessFactor != 0.f) ||
                               m.extClearcoat->clearcoatTexture.index.has_value() || m.extClearcoat->clearcoatRoughnessTexture.index.has_value() ||
                               m.extClearcoat->clearcoatNormalTexture.index.has_value();
            if (nonDefault)
            {
                json cx; if (m.extClearcoat->clearcoatFactor != 0.f)          cx["clearcoatFactor"] = m.extClearcoat->clearcoatFactor;
                if (m.extClearcoat->clearcoatRoughnessFactor != 0.f)          cx["clearcoatRoughnessFactor"] = m.extClearcoat->clearcoatRoughnessFactor;
                if (auto t = TextureRefToJson(pure::TextureRef{m.extClearcoat->clearcoatTexture.index, m.extClearcoat->clearcoatTexture.texCoord, m.extClearcoat->clearcoatTexture.scale, m.extClearcoat->clearcoatTexture.strength}, texRemap); !t.empty())
                    cx["clearcoatTexture"] = std::move(t);
                if (auto t = TextureRefToJson(pure::TextureRef{m.extClearcoat->clearcoatRoughnessTexture.index, m.extClearcoat->clearcoatRoughnessTexture.texCoord, m.extClearcoat->clearcoatRoughnessTexture.scale, m.extClearcoat->clearcoatRoughnessTexture.strength}, texRemap); !t.empty())
                    cx["clearcoatRoughnessTexture"] = std::move(t);
                if (auto t = TextureRefToJson(pure::TextureRef{m.extClearcoat->clearcoatNormalTexture.index, m.extClearcoat->clearcoatNormalTexture.texCoord, m.extClearcoat->clearcoatNormalTexture.scale, m.extClearcoat->clearcoatNormalTexture.strength}, texRemap); !t.empty())
                    cx["clearcoatNormalTexture"] = std::move(t);
                ext["KHR_materials_clearcoat"] = std::move(cx);
            }
        }
        // Sheen (defaults color=[0,0,0], roughness=0)
        if (m.extSheen)
        {
            bool nonDefault = !IsZero3(m.extSheen->sheenColorFactor) || m.extSheen->sheenRoughnessFactor != 0.f ||
                               m.extSheen->sheenColorTexture.index.has_value() || m.extSheen->sheenRoughnessTexture.index.has_value();
            if (nonDefault)
            {
                json sh; if (!IsZero3(m.extSheen->sheenColorFactor))
                    sh["sheenColorFactor"] = { m.extSheen->sheenColorFactor[0], m.extSheen->sheenColorFactor[1], m.extSheen->sheenColorFactor[2] };
                if (m.extSheen->sheenRoughnessFactor != 0.f)
                    sh["sheenRoughnessFactor"] = m.extSheen->sheenRoughnessFactor;
                if (auto t = TextureRefToJson(pure::TextureRef{m.extSheen->sheenColorTexture.index, m.extSheen->sheenColorTexture.texCoord, m.extSheen->sheenColorTexture.scale, m.extSheen->sheenColorTexture.strength}, texRemap); !t.empty())
                    sh["sheenColorTexture"] = std::move(t);
                if (auto t = TextureRefToJson(pure::TextureRef{m.extSheen->sheenRoughnessTexture.index, m.extSheen->sheenRoughnessTexture.texCoord, m.extSheen->sheenRoughnessTexture.scale, m.extSheen->sheenRoughnessTexture.strength}, texRemap); !t.empty())
                    sh["sheenRoughnessTexture"] = std::move(t);
                ext["KHR_materials_sheen"] = std::move(sh);
            }
        }
        // Volume (defaults thickness=0, attenuationColor=[1,1,1], attenuationDistance=+inf)
        if (m.extVolume)
        {
            bool nonDefault = (m.extVolume->thicknessFactor != 0.f) || m.extVolume->thicknessTexture.index.has_value() ||
                               !(m.extVolume->attenuationColor[0]==1.f && m.extVolume->attenuationColor[1]==1.f && m.extVolume->attenuationColor[2]==1.f) ||
                               (m.extVolume->attenuationDistance != std::numeric_limits<float>::infinity());
            if (nonDefault)
            {
                json vo; if (m.extVolume->thicknessFactor != 0.f)
                    vo["thicknessFactor"] = m.extVolume->thicknessFactor;
                if (auto t = TextureRefToJson(pure::TextureRef{m.extVolume->thicknessTexture.index, m.extVolume->thicknessTexture.texCoord, m.extVolume->thicknessTexture.scale, m.extVolume->thicknessTexture.strength}, texRemap); !t.empty())
                    vo["thicknessTexture"] = std::move(t);
                if (!(m.extVolume->attenuationColor[0]==1.f && m.extVolume->attenuationColor[1]==1.f && m.extVolume->attenuationColor[2]==1.f))
                    vo["attenuationColor"] = { m.extVolume->attenuationColor[0], m.extVolume->attenuationColor[1], m.extVolume->attenuationColor[2] };
                if (m.extVolume->attenuationDistance != std::numeric_limits<float>::infinity())
                    vo["attenuationDistance"] = m.extVolume->attenuationDistance;
                ext["KHR_materials_volume"] = std::move(vo);
            }
        }
        // Anisotropy (defaults strength=0, rotation=0)
        if (m.extAnisotropy)
        {
            bool nonDefault = (m.extAnisotropy->strength != 0.f) || (m.extAnisotropy->rotation != 0.f) || m.extAnisotropy->texture.index.has_value();
            if (nonDefault)
            {
                json an; if (m.extAnisotropy->strength != 0.f) an["anisotropyStrength"] = m.extAnisotropy->strength;
                if (m.extAnisotropy->rotation != 0.f) an["anisotropyRotation"] = m.extAnisotropy->rotation;
                if (auto t = TextureRefToJson(pure::TextureRef{m.extAnisotropy->texture.index, m.extAnisotropy->texture.texCoord, m.extAnisotropy->texture.scale, m.extAnisotropy->texture.strength}, texRemap); !t.empty())
                    an["anisotropyTexture"] = std::move(t);
                ext["KHR_materials_anisotropy"] = std::move(an);
            }
        }
        // Iridescence (defaults factor=0, ior=1.3, thicknessMin=100, thicknessMax=400)
        if (m.extIridescence)
        {
            bool nonDefault = (m.extIridescence->factor != 0.f) || (m.extIridescence->ior != 1.3f) ||
                               (m.extIridescence->thicknessMinimum != 100.f) || (m.extIridescence->thicknessMaximum != 400.f) ||
                               m.extIridescence->texture.index.has_value() || m.extIridescence->thicknessTexture.index.has_value();
            if (nonDefault)
            {
                json ir; if (m.extIridescence->factor != 0.f) ir["iridescenceFactor"] = m.extIridescence->factor;
                if (m.extIridescence->ior != 1.3f) ir["iridescenceIor"] = m.extIridescence->ior;
                if (m.extIridescence->thicknessMinimum != 100.f) ir["iridescenceThicknessMinimum"] = m.extIridescence->thicknessMinimum;
                if (m.extIridescence->thicknessMaximum != 400.f) ir["iridescenceThicknessMaximum"] = m.extIridescence->thicknessMaximum;
                if (auto t = TextureRefToJson(pure::TextureRef{m.extIridescence->texture.index, m.extIridescence->texture.texCoord, m.extIridescence->texture.scale, m.extIridescence->texture.strength}, texRemap); !t.empty())
                    ir["iridescenceTexture"] = std::move(t);
                if (auto t = TextureRefToJson(pure::TextureRef{m.extIridescence->thicknessTexture.index, m.extIridescence->thicknessTexture.texCoord, m.extIridescence->thicknessTexture.scale, m.extIridescence->thicknessTexture.strength}, texRemap); !t.empty())
                    ir["iridescenceThicknessTexture"] = std::move(t);
                ext["KHR_materials_iridescence"] = std::move(ir);
            }
        }
        return ext;
    }

    static json MaterialToJson(const gltf::GLTFMaterialImpl &m, const pure::Model &model,
                               const std::unordered_map<std::size_t,int32_t> &texRemap,
                               const std::unordered_map<std::size_t,int32_t> &imgRemap,
                               const std::unordered_map<std::size_t,int32_t> &sampRemap,
                               const std::string &baseName)
    {
        json j;
        if (!m.name.empty()) j["name"] = m.name;
        if (auto pbr = PBRToJson(m.pbr, texRemap); !pbr.empty())
            j["pbrMetallicRoughness"] = std::move(pbr);
        else
            j["pbrMetallicRoughness"] = json::object(); // keep object for stability
        if (m.normalTexture && TextureRefToJson(*m.normalTexture, texRemap))
            j["normalTexture"] = TextureRefToJson(*m.normalTexture, texRemap);
        if (m.occlusionTexture && TextureRefToJson(*m.occlusionTexture, texRemap))
            j["occlusionTexture"] = TextureRefToJson(*m.occlusionTexture, texRemap);
        if (m.emissiveTexture && TextureRefToJson(*m.emissiveTexture, texRemap))
            j["emissiveTexture"] = TextureRefToJson(*m.emissiveTexture, texRemap);
        if (!IsZero3(m.emissiveFactor))
            j["emissiveFactor"] = { m.emissiveFactor[0], m.emissiveFactor[1], m.emissiveFactor[2] };

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

        json ext = ExtensionsToJson(m, texRemap);
        if (!ext.empty()) j["extensions"] = std::move(ext);

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
            const auto &matPtr = model.materials[i];
            const gltf::GLTFMaterialImpl *m = dynamic_cast<const gltf::GLTFMaterialImpl*>(matPtr.get());
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
