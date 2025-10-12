#include "fbx/import/FBXMaterialConverter.h"
#include <fbxsdk.h>
#include <string>

using namespace fbxsdk;

namespace fbx {

static std::string ToString(const FbxString &s) { return std::string(s.Buffer()); }

void ExtractMaterial(FbxSurfaceMaterial* src, pure::FBXMaterial &outRaw, std::optional<pure::PBRMaterial> *outPBR)
{
    outRaw.name = src ? ToString(src->GetName()) : std::string();
    outRaw.impl = src ? src->ClassId.Is(FbxSurfacePhong::ClassId) ? "Phong" : (src->ClassId.Is(FbxSurfaceLambert::ClassId) ? "Lambert" : "Unknown") : "";

    // Iterate properties and store raw values (colors, scalars, texture names)
    FbxProperty prop = src->GetFirstProperty();
    while (prop.IsValid())
    {
        const char* propName = prop.GetName();
        if (prop.GetPropertyDataType().Is(FbxColor3DT))
        {
            FbxDouble3 v = prop.Get<FbxDouble3>();
            outRaw.raw[propName] = { v[0], v[1], v[2] };
        }
        else if (prop.GetPropertyDataType().Is(FbxDoubleDT) || prop.GetPropertyDataType().Is(FbxFloatDT))
        {
            double d = prop.Get<double>();
            outRaw.raw[propName] = d;
        }
        else if (prop.GetPropertyDataType().Is(FbxStringDT))
        {
            outRaw.raw[propName] = ToString(prop.Get<FbxString>());
        }

        // textures
        int lConnectedSrcCount = prop.GetSrcObjectCount<FbxFileTexture>();
        for (int t = 0; t < lConnectedSrcCount; ++t)
        {
            FbxFileTexture* tex = prop.GetSrcObject<FbxFileTexture>(t);
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
