#pragma once

#include <cstdint>
#include <vector>

namespace pure
{
#pragma pack(push, 1)
    struct MeshletDescriptor
    {
        uint32_t vertex_offset;    // start offset in global meshlet_vertices (unit: uint32)
        uint32_t triangle_offset;  // start offset in global meshlet_triangles (unit: u8vec3 / 3 bytes)
        uint8_t  vertex_count;     // local unique vertex count (<= 64)
        uint8_t  triangle_count;   // local micro triangle count (<= 124)
        uint16_t reserved16;       // alignment padding / flags
        uint32_t reserved32;       // pad to 16 bytes (matches BDA buffer_reference_align=16)
    };
    static_assert(sizeof(MeshletDescriptor) == 16, "MeshletDescriptor must be 16 bytes");

    struct MeshletBounds
    {
        float center[3];
        float radius;
        float cone_apex[3];
        float cone_axis[3];
        float cone_cutoff;
        int8_t cone_axis_s8[3];
        int8_t cone_cutoff_s8;
    };
#pragma pack(pop)

    struct MeshletData
    {
        std::vector<MeshletDescriptor> descriptors;
        std::vector<uint32_t>          vertices;
        std::vector<uint8_t>           triangles;   // 3 bytes per triangle (u8vec3)
        std::vector<MeshletBounds>     bounds;
    };
} // namespace pure
