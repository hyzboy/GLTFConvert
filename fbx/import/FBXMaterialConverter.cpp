#include "fbx/import/FBXMaterialConverter.h"
#include <fbxsdk.h>
#include <string>
#include <cstring>

using namespace fbxsdk;

namespace fbx {

static std::string ToString(const FbxString &s) { return std::string(s.Buffer()); }

void ExtractMaterial(FbxSurfaceMaterial* src, pure::FBXMaterial &outRaw, std::optional<pure::PBRMaterial> *outPBR)
{
    outRaw.name = src ? ToString(src->GetName()) : std::string();
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

    // Iterate properties and store raw values (colors, scalars, texture names)
    FbxProperty prop = src->GetFirstProperty();
    while (prop.IsValid())
    {
        const char* propName = prop.GetName();
        // Use EFbxType to avoid direct references to FbxDataType globals
        EFbxType dtype = prop.GetPropertyDataType().GetType();
        if (dtype == eFbxDouble3)
        {
            FbxDouble3 v = prop.Get<FbxDouble3>();
            outRaw.raw[propName] = { v[0], v[1], v[2] };
        }
        else if (dtype == eFbxDouble || dtype == eFbxFloat)
        {
            double d = prop.Get<double>();
            outRaw.raw[propName] = d;
        }
        else if (dtype == eFbxString)
        {
            outRaw.raw[propName] = ToString(prop.Get<FbxString>());
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
            }
        }

        prop = src->GetNextProperty(prop);
    }

    // Optionally try to fill a simple PBR estimate (very basic)
    if (outPBR)
    {
        pure::PBRMaterial pbr;
        // Try to get diffuse color
        FbxProperty diffuse = src->FindProperty("Diffuse");
        if (diffuse.IsValid())
        {
            FbxDouble3 c = diffuse.Get<FbxDouble3>();
            pbr.pbr.baseColorFactor[0] = static_cast<float>(c[0]);
            pbr.pbr.baseColorFactor[1] = static_cast<float>(c[1]);
            pbr.pbr.baseColorFactor[2] = static_cast<float>(c[2]);
            pbr.pbr.baseColorFactor[3] = 1.0f;
        }
        // simple defaults left for other fields
        *outPBR = pbr;
    }
}

}
