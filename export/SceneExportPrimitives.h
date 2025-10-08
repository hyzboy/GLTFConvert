#pragma once

#include <string>
#include "SceneExportData.h"
#include "SceneExportCollect.h"

namespace exporters
{
    // Build exported primitive entries
    void BuildPrimitivesExport(const CollectedIndices &ci,
                               const std::string &geometryBaseName,
                               SceneExportData &outData);
}
