#pragma once

#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>
#include "common/VKFormat.h"

namespace pure
{
    enum class NormalExportFormat
    {
        V2UN8, // Default: octahedral uint8x2 (VK_FORMAT_R8G8_UNORM, 2B/vert)
        V2HF,  // Reserved: octahedral half2 (VK_FORMAT_R16G16_SFLOAT, 4B/vert)
        V3F,   // Raw float3 (VK_FORMAT_R32G32B32_SFLOAT, 12B/vert)
    };

    inline uint16_t FloatToHalf(float f)
    {
        uint32_t x;
        std::memcpy(&x, &f, sizeof(x));
        const uint32_t sign = (x >> 16) & 0x8000u;
        const int32_t exp   = static_cast<int32_t>((x >> 23) & 0xFF) - 127 + 15;
        uint32_t mant = x & 0x7FFFFFu;

        if(exp <= 0)
        {
            if(exp < -10)
                return static_cast<uint16_t>(sign);
            mant |= 0x800000u;
            return static_cast<uint16_t>(sign | (mant >> static_cast<uint32_t>(14 - exp)));
        }
        if(exp >= 31)
            return static_cast<uint16_t>(sign | 0x7C00u);
        return static_cast<uint16_t>(sign | (static_cast<uint32_t>(exp) << 10) | (mant >> 13));
    }

    inline uint8_t QuantizeU8(float v)
    {
        const int32_t q = static_cast<int32_t>(std::roundf(v * 127.5f + 127.5f));
        return static_cast<uint8_t>(q < 0 ? 0 : (q > 255 ? 255 : q));
    }

    inline void EncodeOctahedralNormal(float nx, float ny, float nz, float &out_p, float &out_q)
    {
        const float len = std::sqrt(nx * nx + ny * ny + nz * nz);

        if(len < 0.0001f)
        {
            out_p = 0.0f;
            out_q = 0.0f;
            return;
        }

        nx /= len;
        ny /= len;
        nz /= len;

        const float ax = std::fabs(nx);
        const float ay = std::fabs(ny);
        const float az = std::fabs(nz);
        const float l1 = ax + ay + az;

        out_p = nx / l1;
        out_q = ny / l1;

        if(nz < 0.0f)
        {
            if(out_p * out_p + out_q * out_q < 1e-6f)
            {
                out_p = 1.0f;
                out_q = 1.0f;
            }
            else
            {
                out_p = (1.0f - ay / l1) * (nx >= 0.0f ? 1.0f : -1.0f);
                out_q = (1.0f - ax / l1) * (ny >= 0.0f ? 1.0f : -1.0f);
            }
        }
    }
} // namespace pure
