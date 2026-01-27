#pragma once

#include <vector>
#include <glm/glm.hpp>

#include "SceneExportData.h"

namespace pure { struct Model; }

namespace exporters
{
    void ComputeBounds(const pure::Model &model,
                       const std::vector<glm::mat4> &worldMatrices,
                       SceneExportData &outData);
}
