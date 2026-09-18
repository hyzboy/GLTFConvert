#pragma once

#include <filesystem>
#include "gltf/GLTFModel.h"
#include "common/VertexCompression.h"

namespace gltf
{
    bool ImportFastGLTF(const std::filesystem::path &inputPath,GLTFModel &outModel);
    // Allow enabling uint8 index buffers when importing. Default is false.
    void SetAllowU8Indices(bool allow);
    bool GetAllowU8Indices();

    void SetNormalExportFormat(pure::NormalExportFormat fmt);
    pure::NormalExportFormat GetNormalExportFormat();

    void SetExportTangent(bool allow);
    bool GetExportTangent();
}//namespace gltf
