#pragma once

#include <nlohmann/json.hpp>
#include "math/BoundingVolumes.h"

namespace exporters
{
    // Convert BoundingVolumes to a nlohmann::json object. Empty components are omitted.
    nlohmann::json BoundingVolumesToJson(const BoundingVolumes &bv);
}
