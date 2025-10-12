#pragma once

#include "pure/FBXMaterial.h"
#include "pure/PBRMaterial.h"

// forward-declare FBX SDK types used in header signatures
namespace fbxsdk { class FbxSurfaceMaterial; }

namespace fbx {

// Extract FBX material into pure::FBXMaterial (raw) and optionally to PBRMaterial via converters.
void ExtractMaterial(fbxsdk::FbxSurfaceMaterial* src, pure::FBXMaterial &outRaw, std::optional<pure::PBRMaterial> *outPBR = nullptr);

}
