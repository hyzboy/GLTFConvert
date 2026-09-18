#include <vector>

#include "gltf/GLTFPrimitive.h"
#include "pure/Geometry.h"
#include "pure/GeometryIndicesMeta.h"
#include "gltf/convert/UniqueGeometryMapping.h"
#include "common/VertexCompression.h"
#include "gltf/import/GLTFImporter.h"

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
            dstGeometry.push_back(std::move(pg));
        }
    }
} // namespace gltf
