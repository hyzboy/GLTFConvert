#pragma once

#include <vulkan/vulkan.h>

enum IndexType:int
{
    AUTO = -1,
    U16 = 0,
    U32,
    U8 = VK_INDEX_TYPE_UINT8_EXT,

    ERR = VK_INDEX_TYPE_MAX_ENUM
};
