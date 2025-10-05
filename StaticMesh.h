#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <optional>
#include <string>
#include <cstdint>

#include "gltf/Model.h" // need GLTFModel
#include "pure/Material.h"
#include "pure/MeshNode.h"
#include "pure/Scene.h"
#include "pure/ModelCore.h"

namespace pure {
Model ConvertFromGLTF(const GLTFModel& src);
} // namespace pure
