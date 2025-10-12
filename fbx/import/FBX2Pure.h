#pragma once

#include "fbx/FBXModel.h"
#include "pure/Model.h"

namespace fbx {
    pure::Model ConvertFromFBX(const FBXModel &src);
}
