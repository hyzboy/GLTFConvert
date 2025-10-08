#include "MaterialExporter.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <vector>
#include <filesystem>

#include "pure/Material.h"
#include "gltf/GLTFModel.h" // for texture mapping when available
#include "ExportFileNames.h"

using nlohmann::json;

namespace exporters
{
    // Resolve image index from a texture index if texture list available
    static int ResolveImageIndex(const std::vector<GLTFTextureInfo>* texList,
                                 std::optional<std::size_t> texIdx)
    {
        if (!texList || !texIdx || *texIdx >= texList->size()) return -1;
        const auto &ti = (*texList)[*texIdx];
        if (!ti.image) return -1;
        return static_cast<int>(*ti.image);
    }

    static json TextureToJson(const GLTFTextureRef &t,
                              const std::vector<GLTFTextureInfo>* texList)
    {
        json jt = json::object();
        if (t.index)
        {
            jt["textureIndex"] = static_cast<int>(*t.index);
            if (int img = ResolveImageIndex(texList, t.index); img >= 0)
                jt["imageIndex"] = img;
        }
        jt["texCoord"] = t.texCoord;
        jt["scale"]    = t.scale;
        jt["strength"] = t.strength;
        return jt;
    }

    static json PBRToJson(const GLTFPBRMetallicRoughness &p,
                          const std::vector<GLTFTextureInfo>* texList)
    {
        json j;
        j["baseColorFactor"]        = { p.baseColorFactor[0], p.baseColorFactor[1], p.baseColorFactor[2], p.baseColorFactor[3] };
        j["metallicFactor"]         = p.metallicFactor;
        j["roughnessFactor"]        = p.roughnessFactor;
        j["baseColorTexture"]       = TextureToJson(p.baseColorTexture, texList);
        j["metallicRoughnessTexture"] = TextureToJson(p.metallicRoughnessTexture, texList);
        return j;
    }

    static json MaterialToJson(const pure::Material &m,
                               const std::vector<GLTFTextureInfo>* texList)
    {
        json j;
        if (!m.name.empty()) j["name"] = m.name;
        j["pbrMetallicRoughness"] = PBRToJson(m.pbr, texList);
        j["normalTexture"]        = TextureToJson(m.normalTexture, texList);
        j["occlusionTexture"]     = TextureToJson(m.occlusionTexture, texList);
        j["emissiveTexture"]      = TextureToJson(m.emissiveTexture, texList);
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

        json ext = json::object();
        if (m.extEmissiveStrength)
            ext["KHR_materials_emissive_strength"] = { {"emissiveStrength", m.extEmissiveStrength->emissiveStrength } };
        if (m.extUnlit)
            ext["KHR_materials_unlit"] = json::object();
        if (m.extIOR)
            ext["KHR_materials_ior"] = { {"ior", m.extIOR->ior } };
        if (m.extTransmission)
        {
            json tx;
            tx["transmissionFactor"]  = m.extTransmission->transmissionFactor;
            tx["transmissionTexture"] = TextureToJson(m.extTransmission->transmissionTexture, texList);
            ext["KHR_materials_transmission"] = std::move(tx);
        }
        if (m.extDiffuseTransmission)
        {
            json dx;
            dx["diffuseTransmissionFactor"]  = m.extDiffuseTransmission->diffuseTransmissionFactor;
            dx["diffuseTransmissionTexture"] = TextureToJson(m.extDiffuseTransmission->diffuseTransmissionTexture, texList);
            ext["KHR_materials_diffuse_transmission"] = std::move(dx);
        }
        if (m.extSpecular)
        {
            json sx;
            sx["specularFactor"]      = m.extSpecular->specularFactor;
            sx["specularColorFactor"] = { m.extSpecular->specularColorFactor[0], m.extSpecular->specularColorFactor[1], m.extSpecular->specularColorFactor[2] };
            sx["specularTexture"]     = TextureToJson(m.extSpecular->specularTexture, texList);
            sx["specularColorTexture"] = TextureToJson(m.extSpecular->specularColorTexture, texList);
            ext["KHR_materials_specular"] = std::move(sx);
        }
        if (m.extClearcoat)
        {
            json cx;
            cx["clearcoatFactor"]          = m.extClearcoat->clearcoatFactor;
            cx["clearcoatRoughnessFactor"] = m.extClearcoat->clearcoatRoughnessFactor;
            cx["clearcoatTexture"]         = TextureToJson(m.extClearcoat->clearcoatTexture, texList);
            cx["clearcoatRoughnessTexture"] = TextureToJson(m.extClearcoat->clearcoatRoughnessTexture, texList);
            cx["clearcoatNormalTexture"]   = TextureToJson(m.extClearcoat->clearcoatNormalTexture, texList);
            ext["KHR_materials_clearcoat"] = std::move(cx);
        }
        if (m.extSheen)
        {
            json sh;
            sh["sheenColorFactor"]    = { m.extSheen->sheenColorFactor[0], m.extSheen->sheenColorFactor[1], m.extSheen->sheenColorFactor[2] };
            sh["sheenRoughnessFactor"] = m.extSheen->sheenRoughnessFactor;
            sh["sheenColorTexture"]   = TextureToJson(m.extSheen->sheenColorTexture, texList);
            sh["sheenRoughnessTexture"] = TextureToJson(m.extSheen->sheenRoughnessTexture, texList);
            ext["KHR_materials_sheen"] = std::move(sh);
        }
        if (m.extVolume)
        {
            json vo;
            vo["thicknessFactor"]     = m.extVolume->thicknessFactor;
            vo["thicknessTexture"]    = TextureToJson(m.extVolume->thicknessTexture, texList);
            vo["attenuationColor"]    = { m.extVolume->attenuationColor[0], m.extVolume->attenuationColor[1], m.extVolume->attenuationColor[2] };
            vo["attenuationDistance"] = m.extVolume->attenuationDistance;
            ext["KHR_materials_volume"] = std::move(vo);
        }
        if (m.extAnisotropy)
        {
            json an;
            an["anisotropyStrength"] = m.extAnisotropy->strength;
            an["anisotropyRotation"] = m.extAnisotropy->rotation;
            an["anisotropyTexture"]  = TextureToJson(m.extAnisotropy->texture, texList);
            ext["KHR_materials_anisotropy"] = std::move(an);
        }
        if (m.extIridescence)
        {
            json ir;
            ir["iridescenceFactor"]          = m.extIridescence->factor;
            ir["iridescenceIor"]             = m.extIridescence->ior;
            ir["iridescenceThicknessMinimum"] = m.extIridescence->thicknessMinimum;
            ir["iridescenceThicknessMaximum"] = m.extIridescence->thicknessMaximum;
            ir["iridescenceTexture"]         = TextureToJson(m.extIridescence->texture, texList);
            ir["iridescenceThicknessTexture"] = TextureToJson(m.extIridescence->thicknessTexture, texList);
            ext["KHR_materials_iridescence"] = std::move(ir);
        }
        if (!ext.empty())
            j["extensions"] = std::move(ext);
        return j;
    }

    bool ExportMaterials(const std::vector<pure::Material> &materials,
                         const std::filesystem::path &dir)
    {
        if (materials.empty()) return true;

        std::error_code ec;
        std::filesystem::create_directories(dir, ec);

        const std::vector<GLTFTextureInfo>* texList = nullptr; // future integration point

        // NOTE: We do not have the full model here; baseName is inferred from directory name to stay consistent.
        std::string baseName = dir.filename().string();
        if (auto pos = baseName.find(".StaticMesh"); pos != std::string::npos)
            baseName = baseName.substr(0, pos);

        for (std::size_t i = 0; i < materials.size(); ++i)
        {
            const auto &m = materials[i];
            auto filePath = dir / MakeMaterialFileName(baseName, m.name, static_cast<int32_t>(i));
            std::ofstream ofs(filePath, std::ios::binary);
            if (!ofs)
            {
                std::cerr << "[Export] Cannot write material: " << filePath << "\n";
                return false;
            }
            ofs << MaterialToJson(m, texList).dump(4);
            ofs.close();
            std::cout << "[Export] Saved material: " << filePath << "\n";
        }
        return true;
    }
}
