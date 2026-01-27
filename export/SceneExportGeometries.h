#pragma once

#include <string>

#include "SceneExportData.h"
#include "SceneExportCollect.h"

namespace exporters
{
    void BuildGeometries(const CollectedIndices &ci,
                         const std::string &geometryBaseName,
                         SceneExportData &outData);
}
