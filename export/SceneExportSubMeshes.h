#pragma once

#include <string>

#include "SceneExportData.h"
#include "SceneExportCollect.h"

namespace pure { struct Model; }

namespace exporters
{
    // Build exported primitive entries (legacy function name kept)
    void BuildSubMeshes(const CollectedIndices &ci,
                        const std::string &geometryBaseName,
                        SceneExportData &outData);
}
