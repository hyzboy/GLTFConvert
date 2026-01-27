#pragma once

#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>

#include "SceneExportData.h"
#include "SceneExportCollect.h"
#include "SceneExportTransforms.h"
#include "SceneExportNames.h"

namespace pure { struct Model; }

namespace exporters
{
    void BuildNodes(const pure::Model &model,
                    const CollectedIndices &ci,
                    const RemapTables &remap,
                    const std::vector<glm::mat4> &worldMatrices,
                    std::unordered_map<std::string, int32_t> &nameToIndex,
                    SceneExportData &outData);
}
