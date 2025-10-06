#include <algorithm>
#include <vector>

#include "convert/UniqueGeometryMapping.h"
#include "gltf/Primitive.h"
#include "gltf/Geometry.h"

namespace pure
{
    namespace
    {
        static bool SameGeometryByAccessorId(const GLTFGeometry &a, const GLTFGeometry &b)
        {
            if (a.mode != b.mode) return false;
            if (static_cast<bool>(a.indicesAccessorIndex) != static_cast<bool>(b.indicesAccessorIndex)) return false;
            if (a.indicesAccessorIndex && b.indicesAccessorIndex && (*a.indicesAccessorIndex != *b.indicesAccessorIndex)) return false;
            if (a.attributes.size() != b.attributes.size()) return false;

            auto makeList = [](const GLTFGeometry &g)
            {
                std::vector<std::pair<std::string, std::size_t>> v;
                v.reserve(g.attributes.size());
                for (const auto &at : g.attributes)
                {
                    if (!at.accessorIndex) return std::vector<std::pair<std::string, std::size_t>>{};
                    v.emplace_back(at.name, *at.accessorIndex);
                }
                std::sort(v.begin(), v.end(), [](auto &l, auto &r)
                          {
                              if (l.first != r.first) return l.first < r.first;
                              return l.second < r.second;
                          });
                return v;
            };

            auto la = makeList(a);
            auto lb = makeList(b);
            if (la.empty() || lb.empty()) return false;
            return la == lb;
        }
    } // anonymous namespace

    UniqueGeometryMapping BuildUniqueGeometryMapping(const std::vector<GLTFPrimitive> &prims)
    {
        UniqueGeometryMapping map;
        map.geomIndexOfPrim.assign(prims.size(), -1);
        for (int32_t i = 0; i < static_cast<int32_t>(prims.size()); ++i)
        {
            const auto &g = prims[static_cast<size_t>(i)].geometry;
            int32_t found = -1;
            for (int32_t u = 0; u < static_cast<int32_t>(map.uniqueRepGeomPrimIdx.size()); ++u)
            {
                if (SameGeometryByAccessorId(g, prims[static_cast<size_t>(map.uniqueRepGeomPrimIdx[static_cast<size_t>(u)])].geometry))
                {
                    found = u;
                    break;
                }
            }
            if (found == -1)
            {
                map.uniqueRepGeomPrimIdx.push_back(i);
                map.geomIndexOfPrim[static_cast<size_t>(i)] = static_cast<int32_t>(map.uniqueRepGeomPrimIdx.size()) - 1;
            }
            else
            {
                map.geomIndexOfPrim[static_cast<size_t>(i)] = found;
            }
        }
        return map;
    }
} // namespace pure
