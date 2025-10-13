#pragma once

#include "pure/FBXMaterial.h"
#include "pure/PBRMaterial.h"
#include <memory>

// forward-declare FBX SDK types used in header signatures
namespace fbxsdk { class FbxSurfaceMaterial; }

namespace fbx {

// Extract FBX material into pure::FBXMaterial (raw) and return a concrete pure::Material
// instance when the shading model can be recognized (Phong, Lambert, SpecGloss, PBR).
// If the shading model is unknown, return nullptr and caller may use outRaw as fallback.
std::unique_ptr<pure::Material> ExtractMaterial(fbxsdk::FbxSurfaceMaterial* src, pure::FBXMaterial &outRaw);

}
