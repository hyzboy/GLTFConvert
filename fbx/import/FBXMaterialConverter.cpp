#include "fbx/import/FBXMaterialConverter.h"
#include <fbxsdk.h>
#include <string>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <cctype>
#include "pure/PhongMaterial.h"
#include "pure/LambertMaterial.h"
#include "pure/SpecGlossMaterial.h"
#include <memory>

using namespace fbxsdk;

namespace fbx {

static std::string ToString(const FbxString &s) { return std::string(s.Buffer()); }

std::unique_ptr<pure::Material> ExtractMaterial(FbxSurfaceMaterial* src, pure::FBXMaterial &outRaw)
{
    outRaw.name = src ? ToString(src->GetName()) : std::string();
    std::cout << "Extracting material: " << outRaw.name << std::endl;
    // Determine implementation type without referencing static ClassId symbols (use dynamic_cast)
    outRaw.impl = "Unknown";
    if (src) {
        // Avoid dynamic_cast across DLL boundaries which may crash if RTTI/settings differ.
        // Use runtime class name check instead.
        const char* cname = src->GetClassId().GetName();
        if (cname) {
            std::string cn(cname);
            if (cn.find("FbxSurfacePhong") != std::string::npos || cn.find("SurfacePhong") != std::string::npos) outRaw.impl = "Phong";
            else if (cn.find("FbxSurfaceLambert") != std::string::npos || cn.find("SurfaceLambert") != std::string::npos) outRaw.impl = "Lambert";
        }
    }
    std::cout << "Material implementation type: " << outRaw.impl << std::endl;

    // Iterate properties and store raw values (colors, scalars, texture names)
    FbxProperty prop = src->GetFirstProperty();
    while (prop.IsValid())
    {
        const FbxString propString=prop.GetName();

        const char *propName = propString.Buffer();

        std::cout << "Processing property: " << propName << std::endl;
        // Use EFbxType to avoid direct references to FbxDataType globals
        EFbxType dtype = prop.GetPropertyDataType().GetType();
        if (dtype == eFbxDouble3)
        {
            FbxDouble3 v = prop.Get<FbxDouble3>();
            outRaw.raw[propName] = { v[0], v[1], v[2] };
            std::cout << "  Double3 value: [" << v[0] << ", " << v[1] << ", " << v[2] << "]" << std::endl;
        }
        else if (dtype == eFbxDouble || dtype == eFbxFloat)
        {
            double d = prop.Get<double>();
            outRaw.raw[propName] = d;
            std::cout << "  Scalar value: " << d << std::endl;
        }
        else if (dtype == eFbxString)
        {
            std::string sval = ToString(prop.Get<FbxString>());
            outRaw.raw[propName] = sval;
            std::cout << "  String value: " << sval << std::endl;

            // If this property names the shading model, use it to determine implementation type
            if (std::strcmp(propName, "ShadingModel") == 0)
            {
                std::string lower = sval;
                std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c){ return std::tolower(c); });
                if (lower.find("phong") != std::string::npos) outRaw.impl = "Phong";
                else if (lower.find("lambert") != std::string::npos) outRaw.impl = "Lambert";
                else if (lower.find("spec") != std::string::npos || lower.find("specgloss") != std::string::npos) outRaw.impl = "SpecGloss";
                else if (lower.find("pbr") != std::string::npos || lower.find("metal") != std::string::npos || lower.find("metalrough") != std::string::npos) outRaw.impl = "PBR";
                else outRaw.impl = sval; // unknown shading model, keep raw string
                std::cout << "  Detected shading model (from ShadingModel): " << outRaw.impl << std::endl;
            }
        }

        // textures
        // Iterate source objects and pick up file textures without using templated ClassId-based helpers
        int lConnectedSrcCount = prop.GetSrcObjectCount();
        for (int t = 0; t < lConnectedSrcCount; ++t)
        {
            FbxObject* srcObj = prop.GetSrcObject(t);
            FbxFileTexture* tex = nullptr;
            if (srcObj) {
                const char* scn = srcObj->GetClassId().GetName();
                if (scn && (std::strstr(scn, "FbxFileTexture") || std::strstr(scn, "FileTexture"))) {
                    tex = static_cast<FbxFileTexture*>(srcObj);
                }
            }
            if (tex)
            {
                outRaw.textures.push_back({});
                outRaw.textures.back().index = std::nullopt; // actual texture index mapping later
                outRaw.raw[std::string(propName) + "_texture_" + std::to_string(t)] = ToString(tex->GetFileName());
                std::cout << "  Found texture: " << ToString(tex->GetFileName()) << std::endl;
            }
        }

        prop = src->GetNextProperty(prop);
    }

    // Decide which concrete pure::Material to create based on outRaw.impl
    std::string implLower = outRaw.impl;
    std::transform(implLower.begin(), implLower.end(), implLower.begin(), [](unsigned char c){ return std::tolower(c); });

    // Helper to read vec3 from raw JSON
    auto getVec3FromRaw = [&](const nlohmann::json &r, const char* keys[], size_t keyCount, float out[3]) -> bool {
        for (size_t ki=0; ki<keyCount; ++ki) {
            const char* k = keys[ki];
            if (r.contains(k) && r[k].is_array() && r[k].size()>=3) {
                for (int i=0;i<3;++i) out[i] = static_cast<float>(r[k][i].get<double>());
                return true;
            }
        }
        return false;
    };

    if (implLower.find("phong") != std::string::npos)
    {
        auto m = std::make_unique<pure::PhongMaterial>();
        m->name = outRaw.name;
        // try common keys
        const char* diffKeys[] = {"Diffuse","DiffuseColor","diffuse"};
        const char* specKeys[] = {"Specular","SpecularColor","specular"};
        float tmp[3];
        if (getVec3FromRaw(outRaw.raw, diffKeys, std::size(diffKeys), tmp)) { m->diffuse[0]=tmp[0]; m->diffuse[1]=tmp[1]; m->diffuse[2]=tmp[2]; }
        if (getVec3FromRaw(outRaw.raw, specKeys, std::size(specKeys), tmp)) { m->specular[0]=tmp[0]; m->specular[1]=tmp[1]; m->specular[2]=tmp[2]; }
        if (outRaw.raw.contains("Shininess") && outRaw.raw["Shininess"].is_number()) m->shininess = static_cast<float>(outRaw.raw["Shininess"].get<double>());
        if (!outRaw.textures.empty()) m->diffuseTexture = outRaw.textures[0];
        if (outRaw.textures.size()>1) m->specularTexture = outRaw.textures[1];
        return m;
    }
    else if (implLower.find("lambert") != std::string::npos)
    {
        auto m = std::make_unique<pure::LambertMaterial>();
        m->name = outRaw.name;
        const char* diffKeys[] = {"Diffuse","DiffuseColor","diffuse"};
        const char* ambKeys[] = {"Ambient","ambient"};
        const char* emiKeys[] = {"Emissive","emissive"};
        float tmp[3];
        if (getVec3FromRaw(outRaw.raw, diffKeys, std::size(diffKeys), tmp)) { m->diffuse[0]=tmp[0]; m->diffuse[1]=tmp[1]; m->diffuse[2]=tmp[2]; }
        if (getVec3FromRaw(outRaw.raw, ambKeys, std::size(ambKeys), tmp)) { m->ambient[0]=tmp[0]; m->ambient[1]=tmp[1]; m->ambient[2]=tmp[2]; }
        if (getVec3FromRaw(outRaw.raw, emiKeys, std::size(emiKeys), tmp)) { m->emissive[0]=tmp[0]; m->emissive[1]=tmp[1]; m->emissive[2]=tmp[2]; }
        if (!outRaw.textures.empty()) m->diffuseTexture = outRaw.textures[0];
        return m;
    }
    else if (implLower.find("spec") != std::string::npos)
    {
        auto m = std::make_unique<pure::SpecGlossMaterial>();
        m->name = outRaw.name;
        const char* diffKeys[] = {"Diffuse","DiffuseColor","diffuse"};
        const char* specKeys[] = {"Specular","SpecularColor","specular"};
        float tmp[3];
        if (getVec3FromRaw(outRaw.raw, diffKeys, std::size(diffKeys), tmp)) { m->diffuse[0]=tmp[0]; m->diffuse[1]=tmp[1]; m->diffuse[2]=tmp[2]; }
        if (getVec3FromRaw(outRaw.raw, specKeys, std::size(specKeys), tmp)) { m->specular[0]=tmp[0]; m->specular[1]=tmp[1]; m->specular[2]=tmp[2]; }
        if (outRaw.raw.contains("Glossiness") && outRaw.raw["Glossiness"].is_number()) m->glossiness = static_cast<float>(outRaw.raw["Glossiness"].get<double>());
        if (!outRaw.textures.empty()) m->diffuseTexture = outRaw.textures[0];
        if (outRaw.textures.size()>1) m->specularGlossTexture = outRaw.textures[1];
        return m;
    }
    else if (implLower.find("pbr") != std::string::npos || implLower.find("metal") != std::string::npos)
    {
        // try to produce a PBRMaterial estimate
        auto pm = std::make_unique<pure::PBRMaterial>();
        pm->name = outRaw.name;
        bool pbrValid = false;
        const char* diffKeys[] = {"Diffuse","DiffuseColor","diffuse"};
        float tmp[3];
        if (getVec3FromRaw(outRaw.raw, diffKeys, std::size(diffKeys), tmp)) {
            pm->pbr.baseColorFactor[0] = tmp[0]; pm->pbr.baseColorFactor[1] = tmp[1]; pm->pbr.baseColorFactor[2] = tmp[2]; pm->pbr.baseColorFactor[3] = 1.0f;
            pbrValid = true;
        }
        if (pbrValid) return pm;
    }

    // Unknown shading model -> leave as raw FBX material (caller can use outRaw)
    return nullptr;
}

}
