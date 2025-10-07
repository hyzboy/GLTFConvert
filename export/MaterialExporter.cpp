#include "MaterialExporter.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <vector>
#include <filesystem>

#include "pure/Material.h"

using nlohmann::json;

namespace exporters
{
    static json TextureToJson(const GLTFTextureRef &t)
    {
        json jt = json::object();
        if (t.index) jt["index"] = *t.index;
        jt["texCoord"] = t.texCoord;
        jt["scale"] = t.scale;
        jt["strength"] = t.strength;
        return jt;
    }

    static json PBRToJson(const GLTFPBRMetallicRoughness &p)
    {
        json j;
        j["baseColorFactor"] = { p.baseColorFactor[0], p.baseColorFactor[1], p.baseColorFactor[2], p.baseColorFactor[3] };
        j["metallicFactor"] = p.metallicFactor;
        j["roughnessFactor"] = p.roughnessFactor;
        j["baseColorTexture"] = TextureToJson(p.baseColorTexture);
        j["metallicRoughnessTexture"] = TextureToJson(p.metallicRoughnessTexture);
        return j;
    }

    static json MaterialToJson(const pure::Material &m)
    {
        json j;
        if (!m.name.empty()) j["name"] = m.name;
        j["pbrMetallicRoughness"] = PBRToJson(m.pbr);
        j["normalTexture"] = TextureToJson(m.normalTexture);
        j["occlusionTexture"] = TextureToJson(m.occlusionTexture);
        j["emissiveTexture"] = TextureToJson(m.emissiveTexture);
        j["emissiveFactor"] = { m.emissiveFactor[0], m.emissiveFactor[1], m.emissiveFactor[2] };
        const char *alphaStr = "OPAQUE";
        switch (m.alphaMode) {
            case pure::Material::AlphaMode::Opaque: alphaStr = "OPAQUE"; break;
            case pure::Material::AlphaMode::Mask: alphaStr = "MASK"; break;
            case pure::Material::AlphaMode::Blend: alphaStr = "BLEND"; break;
        }
        j["alphaMode"] = alphaStr;
        j["alphaCutoff"] = m.alphaCutoff;
        j["doubleSided"] = m.doubleSided;
        json ext = json::object();
        if (m.extEmissiveStrength) ext["KHR_materials_emissive_strength"] = { {"emissiveStrength", m.extEmissiveStrength->emissiveStrength } };
        if (m.extUnlit) ext["KHR_materials_unlit"] = json::object();
        if (m.extIOR) ext["KHR_materials_ior"] = { {"ior", m.extIOR->ior} };
        if (m.extTransmission) { json tx; tx["transmissionFactor"] = m.extTransmission->transmissionFactor; tx["transmissionTexture"] = TextureToJson(m.extTransmission->transmissionTexture); ext["KHR_materials_transmission"] = std::move(tx);}        
        if (m.extDiffuseTransmission) { json dx; dx["diffuseTransmissionFactor"] = m.extDiffuseTransmission->diffuseTransmissionFactor; dx["diffuseTransmissionTexture"] = TextureToJson(m.extDiffuseTransmission->diffuseTransmissionTexture); ext["KHR_materials_diffuse_transmission"] = std::move(dx);}        
        if (m.extSpecular) { json sx; sx["specularFactor"] = m.extSpecular->specularFactor; sx["specularColorFactor"] = { m.extSpecular->specularColorFactor[0], m.extSpecular->specularColorFactor[1], m.extSpecular->specularColorFactor[2] }; sx["specularTexture"] = TextureToJson(m.extSpecular->specularTexture); sx["specularColorTexture"] = TextureToJson(m.extSpecular->specularColorTexture); ext["KHR_materials_specular"] = std::move(sx);}        
        if (m.extClearcoat) { json cx; cx["clearcoatFactor"] = m.extClearcoat->clearcoatFactor; cx["clearcoatRoughnessFactor"] = m.extClearcoat->clearcoatRoughnessFactor; cx["clearcoatTexture"] = TextureToJson(m.extClearcoat->clearcoatTexture); cx["clearcoatRoughnessTexture"] = TextureToJson(m.extClearcoat->clearcoatRoughnessTexture); cx["clearcoatNormalTexture"] = TextureToJson(m.extClearcoat->clearcoatNormalTexture); ext["KHR_materials_clearcoat"] = std::move(cx);}        
        if (m.extSheen) { json sh; sh["sheenColorFactor"] = { m.extSheen->sheenColorFactor[0], m.extSheen->sheenColorFactor[1], m.extSheen->sheenColorFactor[2] }; sh["sheenRoughnessFactor"] = m.extSheen->sheenRoughnessFactor; sh["sheenColorTexture"] = TextureToJson(m.extSheen->sheenColorTexture); sh["sheenRoughnessTexture"] = TextureToJson(m.extSheen->sheenRoughnessTexture); ext["KHR_materials_sheen"] = std::move(sh);}        
        if (m.extVolume) { json vo; vo["thicknessFactor"] = m.extVolume->thicknessFactor; vo["thicknessTexture"] = TextureToJson(m.extVolume->thicknessTexture); vo["attenuationColor"] = { m.extVolume->attenuationColor[0], m.extVolume->attenuationColor[1], m.extVolume->attenuationColor[2] }; vo["attenuationDistance"] = m.extVolume->attenuationDistance; ext["KHR_materials_volume"] = std::move(vo);}        
        if (m.extAnisotropy) { json an; an["anisotropyStrength"] = m.extAnisotropy->strength; an["anisotropyRotation"] = m.extAnisotropy->rotation; an["anisotropyTexture"] = TextureToJson(m.extAnisotropy->texture); ext["KHR_materials_anisotropy"] = std::move(an);}        
        if (m.extIridescence) { json ir; ir["iridescenceFactor"] = m.extIridescence->factor; ir["iridescenceIor"] = m.extIridescence->ior; ir["iridescenceThicknessMinimum"] = m.extIridescence->thicknessMinimum; ir["iridescenceThicknessMaximum"] = m.extIridescence->thicknessMaximum; ir["iridescenceTexture"] = TextureToJson(m.extIridescence->texture); ir["iridescenceThicknessTexture"] = TextureToJson(m.extIridescence->thicknessTexture); ext["KHR_materials_iridescence"] = std::move(ir);}        
        if (!ext.empty()) j["extensions"] = std::move(ext);
        return j;
    }

    static std::string Sanitize(const std::string &n)
    { std::string out=n; for(char &c:out){ switch(c){ case '/':case '\\':case ':':case '*':case '?':case '"':case '<':case '>':case '|': c='_'; break; default: break; } } return out; }

    bool ExportMaterials(const std::vector<pure::Material> &materials,const std::filesystem::path &dir)
    {
        if (materials.empty()) {
            std::error_code tmpEc; std::filesystem::remove(dir/"Materials.json", tmpEc);
            return true;
        }
        std::error_code ec; std::filesystem::create_directories(dir, ec);
        std::filesystem::remove(dir/"Materials.json", ec);

        std::string baseName = dir.filename().string();
        auto pos = baseName.find(".StaticMesh");
        if (pos != std::string::npos) baseName = baseName.substr(0,pos);

        for (std::size_t i=0;i<materials.size();++i)
        {
            const auto &m = materials[i];
            const std::string safeName = !m.name.empty() ? Sanitize(m.name) : ("material." + std::to_string(i));
            auto filePath = dir / (baseName + "." + safeName + ".material");
            std::ofstream ofs(filePath, std::ios::binary);
            if (!ofs) { std::cerr << "[Export] Cannot write material: " << filePath << "\n"; return false; }
            ofs << MaterialToJson(m).dump(4);
            ofs.close();
            std::cout << "[Export] Saved material: " << filePath << "\n";
        }
        return true;
    }
}
