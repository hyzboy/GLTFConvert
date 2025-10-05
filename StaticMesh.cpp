#include "StaticMesh.h"
#include "SceneExporter.h"

#include <algorithm>
#include <unordered_map>
#include <cstdint>
#include "MeshBounds.h"

namespace pure
{
    static bool SameGeometryByAccessorId(const gltf::Geometry &a,const gltf::Geometry &b)
    {
        if (a.mode != b.mode)
            return false;
        if (static_cast<bool>(a.indicesAccessorIndex) != static_cast<bool>(b.indicesAccessorIndex))
            return false;
        if (a.indicesAccessorIndex && b.indicesAccessorIndex && (*a.indicesAccessorIndex != *b.indicesAccessorIndex))
            return false;
        if (a.attributes.size() != b.attributes.size())
            return false;
        auto makeList = [](const gltf::Geometry &g)
        {
            std::vector<std::pair<std::string, std::size_t>> v;
            v.reserve(g.attributes.size());
            for (const auto &at : g.attributes)
            {
                if (!at.accessorIndex)
                    return std::vector<std::pair<std::string, std::size_t>>{};
                v.emplace_back(at.name, *at.accessorIndex);
            }
            std::sort(v.begin(), v.end(), [](auto &l, auto &r)
            {
                if (l.first != r.first)
                    return l.first < r.first;
                return l.second < r.second;
            });
            return v;
        };
        auto la = makeList(a);
        auto lb = makeList(b);
        if (la.empty() || lb.empty())
            return false;
        return la == lb;
    }

    static bool Mat4Equal(const glm::mat4 &a, const glm::mat4 &b)
    {
        return a == b;
    }

    static bool IsIdentity(const glm::mat4 &m)
    {
        return Mat4Equal(m, glm::mat4(1.0f));
    }

    static bool TRSEqual(const MeshNodeTransform &a, const MeshNodeTransform &b)
    {
        return a.translation == b.translation && a.rotation == b.rotation && a.scale == b.scale;
    }

    // internBounds implementation
    int32_t Model::internBounds(const BoundingBox &b)
    {
        for (std::size_t i = 0; i < bounds.size(); ++i)
        {
            const auto &e = bounds[i];
            if (e.aabb.min == b.aabb.min && e.aabb.max == b.aabb.max &&
                e.obb.center == b.obb.center && e.obb.axisX == b.obb.axisX && e.obb.axisY == b.obb.axisY && e.obb.axisZ == b.obb.axisZ && e.obb.halfSize == b.obb.halfSize &&
                e.sphere.center == b.sphere.center && e.sphere.radius == b.sphere.radius)
                return static_cast<int32_t>(i);
        }
        bounds.push_back(b);
        return static_cast<int32_t>(bounds.size() - 1);
    }

    int32_t Model::internTRS(const MeshNodeTransform &t)
    {
        for (std::size_t i = 0; i < trsPool.size(); ++i)
            if (TRSEqual(trsPool[i], t))
                return static_cast<int32_t>(i);
        trsPool.push_back(t);
        return static_cast<int32_t>(trsPool.size() - 1);
    }

    int32_t Model::internMatrix(const glm::mat4 &m)
    {
        if (IsIdentity(m))
            return -1; // identity not stored
        for (std::size_t i = 0; i < matrixData.size(); ++i)
            if (Mat4Equal(matrixData[i], m))
                return static_cast<int32_t>(i);
        matrixData.push_back(m);
        return static_cast<int32_t>(matrixData.size() - 1);
    }

    namespace
    {

    static void CopyMaterials(
        Model           &dst,
        const gltf::Model &src
    )
    {
        dst.materials.reserve(src.materials.size());
        for (const auto &m : src.materials)
        {
            pure::Material pm;
            pm.name = m.name;
            dst.materials.push_back(std::move(pm));
        }
    }

    static void CopyScenes(Model &dst,const gltf::Model &src)
    {
        dst.scenes.reserve(src.scenes.size());
        for (const auto &s : src.scenes)
        {
            pure::Scene ps;
            ps.name = s.name;
            ps.nodes.reserve(s.nodes.size());
            for (auto ni : s.nodes)
                ps.nodes.push_back(static_cast<int32_t>(ni));
            BoundingBox bb;
            bb.aabb = s.worldAABB;
            bb.obb.reset();
            bb.sphere.reset();
            ps.boundsIndex = dst.internBounds(bb);
            dst.scenes.push_back(std::move(ps));
        }
    }

    static void CopyMeshNodesAndTransforms(Model &dst,const gltf::Model &src)
    {
        dst.mesh_nodes.reserve(src.nodes.size());
        for (const auto &n : src.nodes)
        {
            pure::MeshNode pn;
            pn.name = n.name;
            pn.children.reserve(n.children.size());
            for (auto c : n.children)
                pn.children.push_back(static_cast<int32_t>(c));
            glm::mat4 local = glm::mat4(n.localMatrix());
            glm::mat4 world = glm::mat4(n.worldMatrix);
            int32_t lidx = dst.internMatrix(local);
            if (lidx >= 0)
                pn.localMatrixIndexPlusOne = lidx + 1;
            else
                pn.localMatrixIndexPlusOne = 0;
            int32_t widx = dst.internMatrix(world);
            if (widx >= 0)
                pn.worldMatrixIndexPlusOne = widx + 1;
            else
                pn.worldMatrixIndexPlusOne = 0;
            if (!n.hasMatrix)
            {
                MeshNodeTransform tf;
                tf.translation = glm::vec3(n.translation);
                tf.rotation = glm::quat(n.rotation);
                tf.scale = glm::vec3(n.scale);
                if (!TRSEqual(tf, MeshNodeTransform{}))
                {
                    int32_t tidx = dst.internTRS(tf);
                    pn.trsIndexPlusOne = tidx + 1;
                }
            }
            pn.boundsIndex = kInvalidBoundsIndex;
            dst.mesh_nodes.push_back(std::move(pn));
        }
    }

    struct UniqueGeometryMapping
    {
        std::vector<int32_t> uniqueRepGeomPrimIdx;
        std::vector<int32_t> geomIndexOfPrim;
    };

    static UniqueGeometryMapping BuildUniqueGeometryMapping(const gltf::Model &src)
    {
        UniqueGeometryMapping map;
        const auto &prims = src.primitives;
        map.geomIndexOfPrim.assign(prims.size(), static_cast<int32_t>(-1));

        for (int32_t i = 0; i < static_cast<int32_t>(prims.size()); ++i)
        {
            const auto &g = prims[static_cast<std::size_t>(i)].geometry;
            int32_t found = -1;

            for (int32_t u = 0; u < static_cast<int32_t>(map.uniqueRepGeomPrimIdx.size()); ++u)
            {
                if (SameGeometryByAccessorId(g, prims[static_cast<std::size_t>(map.uniqueRepGeomPrimIdx[static_cast<std::size_t>(u)])].geometry))
                {
                    found = u;
                    break;
                }
            }

            if (found == -1)
            {
                map.uniqueRepGeomPrimIdx.push_back(i);
                map.geomIndexOfPrim[static_cast<std::size_t>(i)] = static_cast<int32_t>(map.uniqueRepGeomPrimIdx.size() - 1);
            }
            else
                map.geomIndexOfPrim[static_cast<std::size_t>(i)] = found;
        }
        return map;
    }

    static void CreateUniqueGeometryEntries(
        Model                 &dst,
        const gltf::Model     &src,
        const UniqueGeometryMapping &map)
    {
        const auto &prims = src.primitives;

        dst.geometry.reserve(map.uniqueRepGeomPrimIdx.size());

        for (int32_t u = 0; u < static_cast<int32_t>(map.uniqueRepGeomPrimIdx.size()); ++u)
        {
            const int32_t i = map.uniqueRepGeomPrimIdx[static_cast<std::size_t>(u)];
            const auto &g = prims[static_cast<std::size_t>(i)].geometry;
            pure::Geometry pg;

            pg.mode = g.mode;
            pg.attributes.reserve(g.attributes.size());

            for (std::size_t ai = 0; ai < g.attributes.size(); ++ai)
            {
                const auto &a = g.attributes[ai];
                pure::GeometryAttribute ga;
                ga.id = static_cast<int64_t>(ai);
                ga.name = a.name;
                ga.count = a.count;
                ga.componentType = a.componentType;
                ga.type = a.type;
                ga.format = a.format;
                ga.data = a.data;
                pg.attributes.push_back(std::move(ga));
            }

            ComputeGeometryBoundsFromGLTF(dst, g, pg);

            if (g.indices)
                pg.indicesData = *g.indices;

            if (g.indexCount && g.indexComponentType)
            {
                pg.indices = pure::GeometryIndicesMeta{ *g.indexCount, *g.indexComponentType };
                pg.indices->indexType = g.indexType;
            }

            dst.geometry.push_back(std::move(pg));
        }
    }

    static void BuildSubMeshes(
        Model                 &dst,
        const gltf::Model     &src,
        const UniqueGeometryMapping &map
    )
    {
        const auto &prims = src.primitives;
        dst.subMeshes.reserve(prims.size());
        for (int32_t i = 0; i < static_cast<int32_t>(prims.size()); ++i)
        {
            const auto &p = prims[static_cast<std::size_t>(i)];
            pure::SubMesh sm;
            sm.geometry = map.geomIndexOfPrim[static_cast<std::size_t>(i)];
            sm.material = p.material ? std::optional<int32_t>(static_cast<int32_t>(*p.material)) : std::optional<int32_t>{};
            dst.subMeshes.push_back(std::move(sm));
        }
    }

    static void AttachNodeSubMeshes(Model &dst,const gltf::Model &src)
    {
        for (int32_t ni = 0; ni < static_cast<int32_t>(src.nodes.size()); ++ni)
        {
            const auto &node = src.nodes[static_cast<std::size_t>(ni)];
            auto &pn = dst.mesh_nodes[static_cast<std::size_t>(ni)];
            if (node.mesh)
            {
                const auto &mesh = src.meshes[static_cast<std::size_t>(*node.mesh)];
                pn.subMeshes.reserve(mesh.primitives.size());
                for (auto primIndex : mesh.primitives)
                    pn.subMeshes.push_back(static_cast<int32_t>(primIndex));
            }
        }
    }

    static void ComputeAllBounds(Model &dst)
    {
        ComputeAllMeshNodeBounds(dst);
        ComputeAllSceneBounds(dst);
    }

    } // anonymous namespace

    Model ConvertFromGLTF(const gltf::Model &src)
    {
        Model dst;
        dst.gltf_source = src.source;
        CopyMaterials(dst, src);
        CopyScenes(dst, src);
        CopyMeshNodesAndTransforms(dst, src);
        auto uniqueMap = BuildUniqueGeometryMapping(src);
        CreateUniqueGeometryEntries(dst, src, uniqueMap);
        BuildSubMeshes(dst, src, uniqueMap);
        AttachNodeSubMeshes(dst, src);
        ComputeAllBounds(dst);
        return dst;
    }
} // namespace pure
