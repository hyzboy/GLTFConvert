#pragma once

#include <string>

#include "SceneExportData.h"
#include "SceneExportCollect.h"

namespace pure { struct Model; }

namespace exporters
{
    void BuildSubMeshes(const pure::Model &model,
                        const CollectedIndices &ci,
                        const RemapTables &remap,
                        const std::string &geometryBaseName,
                        SceneExportData &outData);
}
