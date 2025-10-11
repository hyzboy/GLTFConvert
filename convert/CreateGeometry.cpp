#include <vector>

#include "gltf/GLTFPrimitive.h"
#include "pure/Geometry.h"
#include "pure/GeometryIndicesMeta.h"
#include "convert/UniqueGeometryMapping.h"

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
                // Copy slice derived -> base (data members from GeometryAttribute portion)
                pg.attributes.insert(pg.attributes.end(), g.attributes.begin(), g.attributes.end());

                for(const GeometryAttribute &ga:pg.attributes)
                {
                    if(ga.name=="POSITION" && !ga.data.empty())
                    {
                        if(ga.format==VK_FORMAT_R32G32B32_SFLOAT)
                        {
                            const size_t count=ga.data.size()/(sizeof(float)*3);
                            pg.positions=std::vector<glm::vec3>(count);
                            
                            // Cannot use memcpy directly because glm::vec3 may have 16-byte alignment (AVX2)
                            // while source data is tightly packed (12 bytes per vec3)
                            const float *sp = reinterpret_cast<const float*>(ga.data.data());
                            glm::vec3 *tp = pg.positions->data();
                            
                            for(size_t i = 0; i < count; i++)
                            {
                                tp[i].x = sp[i * 3 + 0];
                                tp[i].y = sp[i * 3 + 1];
                                tp[i].z = sp[i * 3 + 2];
                            }
                        }
                        else
                        if(ga.format==VK_FORMAT_R32G32B32A32_SFLOAT)
                        {
                            const size_t count=ga.data.size()/(sizeof(float)*4);
                            pg.positions=std::vector<glm::vec3>(count);

                            const float *sp=reinterpret_cast<const float*>(ga.data.data());
                            glm::vec3 *tp=pg.positions->data();

                            for(size_t i=0;i<count;i++)
                            {
                                tp[i].x = sp[i * 4 + 0];
                                tp[i].y = sp[i * 4 + 1];
                                tp[i].z = sp[i * 4 + 2];
                                // Skip w component (sp[i * 4 + 3])
                            }
                        }
                        else
                        {
                            // Unsupported POSITION format for decoding
                            pg.positions=std::nullopt;
                        }
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
