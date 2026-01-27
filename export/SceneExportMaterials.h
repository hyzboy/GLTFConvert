#pragma once

#include <unordered_map>
#include <string>

#include "SceneExportData.h"
#include "SceneExportCollect.h"
#include "SceneExportNames.h"

namespace pure { struct Model; }

namespace exporters
{
    void BuildMaterials(const pure::Model &model,
                        const CollectedIndices &ci,
                        std::unordered_map<std::string, int32_t> &nameToIndex,
                        SceneExportData &outData);
}
