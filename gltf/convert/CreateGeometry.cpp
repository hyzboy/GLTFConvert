#include <vector>

#include "gltf/GLTFPrimitive.h"
#include "pure/Geometry.h"
#include "pure/GeometryIndicesMeta.h"
#include "gltf/convert/UniqueGeometryMapping.h"
#include "common/VertexCompression.h"
#include "gltf/import/GLTFImporter.h"
#include <meshoptimizer.h>

namespace gltf
{
    void CreateUniqueGeometryEntries(std::vector<pure::Geometry> &dstGeometry, const std::vector<GLTFPrimitive> &prims, const UniqueGeometryMapping &map)
    {
        dstGeometry.reserve(map.uniqueRepGeomPrimIdx.size());
        for (int32_t u = 0; u < static_cast<int32_t>(map.uniqueRepGeomPrimIdx.size()); ++u)
        {
            int32_t i = map.uniqueRepGeomPrimIdx[static_cast<size_t>(u)];
            const auto &g = prims[static_cast<size_t>(i)].geometry;

            pure::Geometry pg;
            pg.primitiveType = g.primitiveType;

            if (!g.attributes.empty())
            {
                pg.attributes.reserve(g.attributes.size());

                for(const auto &ga : g.attributes)
                {
                    if(ga.name == "POSITION")
                    {
                        if(!ga.data.empty())
                        {
                            if(ga.format == VK_FORMAT_R32G32B32_SFLOAT)
                            {
                                const size_t count = ga.data.size() / (sizeof(float) * 3);
                                pg.positions = std::vector<glm::vec3>(count);
                                const float *sp = reinterpret_cast<const float*>(ga.data.data());
                                glm::vec3 *tp = pg.positions->data();
                                for(size_t v = 0; v < count; ++v)
                                {
                                    tp[v].x = sp[v * 3 + 0];
                                    tp[v].y = sp[v * 3 + 1];
                                    tp[v].z = sp[v * 3 + 2];
                                }
                            }
                            else if(ga.format == VK_FORMAT_R32G32B32A32_SFLOAT)
                            {
                                const size_t count = ga.data.size() / (sizeof(float) * 4);
                                pg.positions = std::vector<glm::vec3>(count);
                                const float *sp = reinterpret_cast<const float*>(ga.data.data());
                                glm::vec3 *tp = pg.positions->data();
                                for(size_t v = 0; v < count; ++v)
                                {
                                    tp[v].x = sp[v * 4 + 0];
                                    tp[v].y = sp[v * 4 + 1];
                                    tp[v].z = sp[v * 4 + 2];
                                }
                            }
                            else
                            {
                                pg.positions = std::nullopt;
                            }
                        }
                        pg.attributes.push_back(ga);
                    }
                    else if(ga.name == "NORMAL")
                    {
                        const auto norm_fmt = GetNormalExportFormat();
                        if(norm_fmt == pure::NormalExportFormat::V2UN8 && ga.format == VK_FORMAT_R32G32B32_SFLOAT)
                        {
                            GeometryAttribute compressed_normal{};
                            compressed_normal.name = "NORMAL";
                            compressed_normal.count = ga.count;
                            compressed_normal.format = VK_FORMAT_R8G8_UNORM;
                            compressed_normal.data.resize(ga.count * 2);
                            const float *src = reinterpret_cast<const float*>(ga.data.data());
                            uint8_t *dst = reinterpret_cast<uint8_t*>(compressed_normal.data.data());
                            for(size_t v = 0; v < ga.count; ++v)
                            {
                                float p, q;
                                pure::EncodeOctahedralNormal(src[v*3+0], src[v*3+1], src[v*3+2], p, q);
                                dst[v*2+0] = pure::QuantizeU8(p);
                                dst[v*2+1] = pure::QuantizeU8(q);
                            }
                            pg.attributes.push_back(std::move(compressed_normal));
                        }
                        else if(norm_fmt == pure::NormalExportFormat::V2HF && ga.format == VK_FORMAT_R32G32B32_SFLOAT)
                        {
                            GeometryAttribute compressed_normal{};
                            compressed_normal.name = "NORMAL";
                            compressed_normal.count = ga.count;
                            compressed_normal.format = VK_FORMAT_R16G16_SFLOAT;
                            compressed_normal.data.resize(ga.count * 4);
                            const float *src = reinterpret_cast<const float*>(ga.data.data());
                            uint16_t *dst = reinterpret_cast<uint16_t*>(compressed_normal.data.data());
                            for(size_t v = 0; v < ga.count; ++v)
                            {
                                float p, q;
                                pure::EncodeOctahedralNormal(src[v*3+0], src[v*3+1], src[v*3+2], p, q);
                                dst[v*2+0] = pure::FloatToHalf(p);
                                dst[v*2+1] = pure::FloatToHalf(q);
                            }
                            pg.attributes.push_back(std::move(compressed_normal));
                        }
                        else
                        {
                            pg.attributes.push_back(ga);
                        }
                    }
                    else if(ga.name.rfind("TEXCOORD", 0) == 0)
                    {
                        if(ga.format == VK_FORMAT_R32G32_SFLOAT)
                        {
                            GeometryAttribute compressed_uv{};
                            compressed_uv.name = ga.name;
                            compressed_uv.count = ga.count;
                            compressed_uv.format = VK_FORMAT_R16G16_SFLOAT;
                            compressed_uv.data.resize(ga.count * 4);
                            const float *src = reinterpret_cast<const float*>(ga.data.data());
                            uint16_t *dst = reinterpret_cast<uint16_t*>(compressed_uv.data.data());
                            for(size_t v = 0; v < ga.count; ++v)
                            {
                                dst[v*2+0] = pure::FloatToHalf(src[v*2+0]);
                                dst[v*2+1] = pure::FloatToHalf(src[v*2+1]);
                            }
                            pg.attributes.push_back(std::move(compressed_uv));
                        }
                        else
                        {
                            pg.attributes.push_back(ga);
                        }
                    }
                    else if(ga.name == "TANGENT")
                    {
                        if(GetExportTangent())
                        {
                            pg.attributes.push_back(ga);
                        }
                        // Default: omitted
                    }
                    else
                    {
                        pg.attributes.push_back(ga);
                    }
                }
            }

            if (g.indices)
                pg.indicesData = *g.indices;
            if (g.indexCount && g.indexType != IndexType::ERR)
            {
                pg.indices = pure::GeometryIndicesMeta{ *g.indexCount, g.indexType };
            }

            // Build Meshlets if enabled and valid mesh data present
            if (GetBuildMeshlets() && pg.positions.has_value() && !pg.positions->empty() && pg.indicesData.has_value() && pg.indices.has_value())
            {
                const size_t vertex_count = pg.positions->size();
                const size_t index_count = pg.indices->count;

                if (index_count >= 3 && pg.indices->indexType == IndexType::U32)
                {
                    const uint32_t *indices = reinterpret_cast<const uint32_t*>(pg.indicesData->data());
                    const float *positions = reinterpret_cast<const float*>(pg.positions->data());

                    constexpr size_t max_vertices = 64;
                    constexpr size_t max_triangles = 124;
                    constexpr float cone_weight = 0.5f;

                    const size_t max_meshlets = meshopt_buildMeshletsBound(index_count, max_vertices, max_triangles);

                    std::vector<meshopt_Meshlet> opt_meshlets(max_meshlets);
                    std::vector<unsigned int> meshlet_vertices(index_count);
                    std::vector<unsigned char> meshlet_triangles(index_count);

                    const size_t meshlet_count = meshopt_buildMeshlets(
                        opt_meshlets.data(),
                        meshlet_vertices.data(),
                        meshlet_triangles.data(),
                        indices,
                        index_count,
                        positions,
                        vertex_count,
                        sizeof(glm::vec3),
                        max_vertices,
                        max_triangles,
                        cone_weight);

                    if (meshlet_count > 0)
                    {
                        pure::MeshletData md;
                        md.descriptors.reserve(meshlet_count);
                        md.bounds.reserve(meshlet_count);

                        const auto &last_meshlet = opt_meshlets[meshlet_count - 1];
                        const size_t total_meshlet_vertices = last_meshlet.vertex_offset + last_meshlet.vertex_count;
                        const size_t total_meshlet_triangles = last_meshlet.triangle_offset + last_meshlet.triangle_count * 3;

                        md.vertices.assign(meshlet_vertices.begin(), meshlet_vertices.begin() + total_meshlet_vertices);
                        md.triangles.assign(meshlet_triangles.begin(), meshlet_triangles.begin() + total_meshlet_triangles);

                        for (size_t mi = 0; mi < meshlet_count; ++mi)
                        {
                            const auto &om = opt_meshlets[mi];
                            pure::MeshletDescriptor desc{};
                            desc.vertex_offset = om.vertex_offset;
                            desc.triangle_offset = om.triangle_offset / 3; // Index in u8vec3 / triangle units
                            desc.vertex_count = static_cast<uint8_t>(om.vertex_count);
                            desc.triangle_count = static_cast<uint8_t>(om.triangle_count);
                            desc.reserved16 = 0;
                            desc.reserved32 = 0;
                            md.descriptors.push_back(desc);

                            meshopt_Bounds b = meshopt_computeMeshletBounds(
                                &meshlet_vertices[om.vertex_offset],
                                &meshlet_triangles[om.triangle_offset],
                                om.triangle_count,
                                positions,
                                vertex_count,
                                sizeof(glm::vec3));

                            pure::MeshletBounds mb{};
                            mb.center[0] = b.center[0];
                            mb.center[1] = b.center[1];
                            mb.center[2] = b.center[2];
                            mb.radius = b.radius;
                            mb.cone_apex[0] = b.cone_apex[0];
                            mb.cone_apex[1] = b.cone_apex[1];
                            mb.cone_apex[2] = b.cone_apex[2];
                            mb.cone_axis[0] = b.cone_axis[0];
                            mb.cone_axis[1] = b.cone_axis[1];
                            mb.cone_axis[2] = b.cone_axis[2];
                            mb.cone_cutoff = b.cone_cutoff;
                            mb.cone_axis_s8[0] = b.cone_axis_s8[0];
                            mb.cone_axis_s8[1] = b.cone_axis_s8[1];
                            mb.cone_axis_s8[2] = b.cone_axis_s8[2];
                            mb.cone_cutoff_s8 = b.cone_cutoff_s8;
                            md.bounds.push_back(mb);
                        }

                        pg.meshlets = std::move(md);
                    }
                }
            }

            dstGeometry.push_back(std::move(pg));
        }
    }
} // namespace gltf
