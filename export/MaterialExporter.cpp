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

    static json TextureRefToJson(const GLTFTextureRef &ref,
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

    static json PBRToJson(const GLTFPBRMetallicRoughness &p,
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
        const auto& gm = m.gltfMaterial;
        // KHR_materials_emissive_strength (default emissiveStrength = 1.0)
        if (gm.extEmissiveStrength && gm.extEmissiveStrength->emissiveStrength != 1.0f)
            ext["KHR_materials_emissive_strength"] = { {"emissiveStrength", gm.extEmissiveStrength->emissiveStrength } };
        // KHR_materials_unlit has no properties (presence is the flag)
        if (gm.extUnlit)
            ext["KHR_materials_unlit"] = json::object();
        // KHR_materials_ior (default ior = 1.5)
        if (gm.extIOR && gm.extIOR->ior != 1.5f)
            ext["KHR_materials_ior"] = { {"ior", gm.extIOR->ior } };
        // KHR_materials_transmission (factor default 0)
        if (gm.extTransmission)
        {
            bool hasTex = gm.extTransmission->transmissionTexture.index.has_value();
            if (gm.extTransmission->transmissionFactor != 0.f || hasTex)
            {
                json tx; if (gm.extTransmission->transmissionFactor != 0.f)
                    tx["transmissionFactor"] = gm.extTransmission->transmissionFactor;
                if (auto t = TextureRefToJson(gm.extTransmission->transmissionTexture, texRemap); !t.empty())
                    tx["transmissionTexture"] = std::move(t);
                ext["KHR_materials_transmission"] = std::move(tx);
            }
        }
        // Diffuse transmission (factor default 0)
        if (gm.extDiffuseTransmission)
        {
            bool hasTex = gm.extDiffuseTransmission->diffuseTransmissionTexture.index.has_value();
            if (gm.extDiffuseTransmission->diffuseTransmissionFactor != 0.f || hasTex)
            {
                json dx; if (gm.extDiffuseTransmission->diffuseTransmissionFactor != 0.f)
                    dx["diffuseTransmissionFactor"]  = gm.extDiffuseTransmission->diffuseTransmissionFactor;
                if (auto t = TextureRefToJson(gm.extDiffuseTransmission->diffuseTransmissionTexture, texRemap); !t.empty())
                    dx["diffuseTransmissionTexture"] = std::move(t);
                ext["KHR_materials_diffuse_transmission"] = std::move(dx);
            }
        }
        // Specular (defaults: specularFactor=1, specularColorFactor=[1,1,1])
        if (gm.extSpecular)
        {
            bool nonDefaultFactor = gm.extSpecular->specularFactor != 1.f;
            bool nonDefaultColor = !(gm.extSpecular->specularColorFactor[0]==1.f && gm.extSpecular->specularColorFactor[1]==1.f && gm.extSpecular->specularColorFactor[2]==1.f);
            bool hasTex = gm.extSpecular->specularTexture.index.has_value() || gm.extSpecular->specularColorTexture.index.has_value();
            if (nonDefaultFactor || nonDefaultColor || hasTex)
            {
                json sx; if (nonDefaultFactor) sx["specularFactor"] = gm.extSpecular->specularFactor;
                if (nonDefaultColor) sx["specularColorFactor"] = { gm.extSpecular->specularColorFactor[0], gm.extSpecular->specularColorFactor[1], gm.extSpecular->specularColorFactor[2] };
                if (auto t = TextureRefToJson(gm.extSpecular->specularTexture, texRemap); !t.empty())
                    sx["specularTexture"] = std::move(t);
                if (auto t = TextureRefToJson(gm.extSpecular->specularColorTexture, texRemap); !t.empty())
                    sx["specularColorTexture"] = std::move(t);
                ext["KHR_materials_specular"] = std::move(sx);
            }
        }
        // Clearcoat (defaults factors 0)
        if (gm.extClearcoat)
        {
            bool nonDefault = (gm.extClearcoat->clearcoatFactor != 0.f) || (gm.extClearcoat->clearcoatRoughnessFactor != 0.f) ||
                               gm.extClearcoat->clearcoatTexture.index.has_value() || gm.extClearcoat->clearcoatRoughnessTexture.index.has_value() ||
                               gm.extClearcoat->clearcoatNormalTexture.index.has_value();
            if (nonDefault)
            {
                json cx; if (gm.extClearcoat->clearcoatFactor != 0.f)          cx["clearcoatFactor"] = gm.extClearcoat->clearcoatFactor;
                if (gm.extClearcoat->clearcoatRoughnessFactor != 0.f)          cx["clearcoatRoughnessFactor"] = gm.extClearcoat->clearcoatRoughnessFactor;
                if (auto t = TextureRefToJson(gm.extClearcoat->clearcoatTexture, texRemap); !t.empty())
                    cx["clearcoatTexture"] = std::move(t);
                if (auto t = TextureRefToJson(gm.extClearcoat->clearcoatRoughnessTexture, texRemap); !t.empty())
                    cx["clearcoatRoughnessTexture"] = std::move(t);
                if (auto t = TextureRefToJson(gm.extClearcoat->clearcoatNormalTexture, texRemap); !t.empty())
                    cx["clearcoatNormalTexture"] = std::move(t);
                ext["KHR_materials_clearcoat"] = std::move(cx);
            }
        }
        // Sheen (defaults color=[0,0,0], roughness=0)
        if (gm.extSheen)
        {
            bool nonDefault = !IsZero3(gm.extSheen->sheenColorFactor) || gm.extSheen->sheenRoughnessFactor != 0.f ||
                               gm.extSheen->sheenColorTexture.index.has_value() || gm.extSheen->sheenRoughnessTexture.index.has_value();
            if (nonDefault)
            {
                json sh; if (!IsZero3(gm.extSheen->sheenColorFactor))
                    sh["sheenColorFactor"] = { gm.extSheen->sheenColorFactor[0], gm.extSheen->sheenColorFactor[1], gm.extSheen->sheenColorFactor[2] };
                if (gm.extSheen->sheenRoughnessFactor != 0.f)
                    sh["sheenRoughnessFactor"] = gm.extSheen->sheenRoughnessFactor;
                if (auto t = TextureRefToJson(gm.extSheen->sheenColorTexture, texRemap); !t.empty())
                    sh["sheenColorTexture"] = std::move(t);
                if (auto t = TextureRefToJson(gm.extSheen->sheenRoughnessTexture, texRemap); !t.empty())
                    sh["sheenRoughnessTexture"] = std::move(t);
                ext["KHR_materials_sheen"] = std::move(sh);
            }
        }
        // Volume (defaults thickness=0, attenuationColor=[1,1,1], attenuationDistance=+inf)
        if (gm.extVolume)
        {
            bool nonDefault = (gm.extVolume->thicknessFactor != 0.f) || gm.extVolume->thicknessTexture.index.has_value() ||
                               !(gm.extVolume->attenuationColor[0]==1.f && gm.extVolume->attenuationColor[1]==1.f && gm.extVolume->attenuationColor[2]==1.f) ||
                               (gm.extVolume->attenuationDistance != std::numeric_limits<float>::infinity());
            if (nonDefault)
            {
                json vo; if (gm.extVolume->thicknessFactor != 0.f)
                    vo["thicknessFactor"] = gm.extVolume->thicknessFactor;
                if (auto t = TextureRefToJson(gm.extVolume->thicknessTexture, texRemap); !t.empty())
                    vo["thicknessTexture"] = std::move(t);
                if (!(gm.extVolume->attenuationColor[0]==1.f && gm.extVolume->attenuationColor[1]==1.f && gm.extVolume->attenuationColor[2]==1.f))
                    vo["attenuationColor"] = { gm.extVolume->attenuationColor[0], gm.extVolume->attenuationColor[1], gm.extVolume->attenuationColor[2] };
                if (gm.extVolume->attenuationDistance != std::numeric_limits<float>::infinity())
                    vo["attenuationDistance"] = gm.extVolume->attenuationDistance;
                ext["KHR_materials_volume"] = std::move(vo);
            }
        }
        // Anisotropy (defaults strength=0, rotation=0)
        if (gm.extAnisotropy)
        {
            bool nonDefault = (gm.extAnisotropy->strength != 0.f) || (gm.extAnisotropy->rotation != 0.f) || gm.extAnisotropy->texture.index.has_value();
            if (nonDefault)
            {
                json an; if (gm.extAnisotropy->strength != 0.f) an["anisotropyStrength"] = gm.extAnisotropy->strength;
                if (gm.extAnisotropy->rotation != 0.f) an["anisotropyRotation"] = gm.extAnisotropy->rotation;
                if (auto t = TextureRefToJson(gm.extAnisotropy->texture, texRemap); !t.empty())
                    an["anisotropyTexture"] = std::move(t);
                ext["KHR_materials_anisotropy"] = std::move(an);
            }
        }
        // Iridescence (defaults factor=0, ior=1.3, thicknessMin=100, thicknessMax=400)
        if (gm.extIridescence)
        {
            bool nonDefault = (gm.extIridescence->factor != 0.f) || (gm.extIridescence->ior != 1.3f) ||
                               (gm.extIridescence->thicknessMinimum != 100.f) || (gm.extIridescence->thicknessMaximum != 400.f) ||
                               gm.extIridescence->texture.index.has_value() || gm.extIridescence->thicknessTexture.index.has_value();
            if (nonDefault)
            {
                json ir; if (gm.extIridescence->factor != 0.f) ir["iridescenceFactor"] = gm.extIridescence->factor;
                if (gm.extIridescence->ior != 1.3f) ir["iridescenceIor"] = gm.extIridescence->ior;
                if (gm.extIridescence->thicknessMinimum != 100.f) ir["iridescenceThicknessMinimum"] = gm.extIridescence->thicknessMinimum;
                if (gm.extIridescence->thicknessMaximum != 400.f) ir["iridescenceThicknessMaximum"] = gm.extIridescence->thicknessMaximum;
                if (auto t = TextureRefToJson(gm.extIridescence->texture, texRemap); !t.empty())
                    ir["iridescenceTexture"] = std::move(t);
                if (auto t = TextureRefToJson(gm.extIridescence->thicknessTexture, texRemap); !t.empty())
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
        const auto& gm = m.gltfMaterial;
        if (!m.name.empty()) j["name"] = m.name;
        if (auto pbr = PBRToJson(gm.pbr, texRemap); !pbr.empty())
            j["pbrMetallicRoughness"] = std::move(pbr);
        else
            j["pbrMetallicRoughness"] = json::object(); // keep object for stability
        if (auto nt = TextureRefToJson(gm.normalTexture, texRemap); !nt.empty())
            j["normalTexture"] = std::move(nt);
        if (auto ot = TextureRefToJson(gm.occlusionTexture, texRemap); !ot.empty())
            j["occlusionTexture"] = std::move(ot);
        if (auto et = TextureRefToJson(gm.emissiveTexture, texRemap); !et.empty())
            j["emissiveTexture"] = std::move(et);
        if (!IsZero3(gm.emissiveFactor))
            j["emissiveFactor"] = { gm.emissiveFactor[0], gm.emissiveFactor[1], gm.emissiveFactor[2] };

        // alphaMode (only if not OPAQUE)
        switch (gm.alphaMode)
        {
            case GLTFMaterial::AlphaMode::Opaque: break;
            case GLTFMaterial::AlphaMode::Mask:
                j["alphaMode"] = "MASK";
                if (gm.alphaCutoff != 0.5f) j["alphaCutoff"] = gm.alphaCutoff; // cutoff only meaningful here
                break;
            case GLTFMaterial::AlphaMode::Blend:
                j["alphaMode"] = "BLEND"; break;
        }
        if (gm.doubleSided) j["doubleSided"] = true;

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
