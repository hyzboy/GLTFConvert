#include "StaticMesh.h"
#include "SceneLocal.h"

#include <algorithm>
#include <unordered_map>
#include <cstdint>
#include "MeshBounds.h"

namespace pure {

// Compare two primitives' geometry by accessor identity (mode, attributes set, indices accessor)
static bool SameGeometryByAccessorId(const gltf::Geometry& a, const gltf::Geometry& b) {
    if (a.mode != b.mode) return false;

    // indices presence and identity
    if (static_cast<bool>(a.indicesAccessorIndex) != static_cast<bool>(b.indicesAccessorIndex)) return false;
    if (a.indicesAccessorIndex && b.indicesAccessorIndex && (*a.indicesAccessorIndex != *b.indicesAccessorIndex)) return false;

    if (a.attributes.size() != b.attributes.size()) return false;

    // Build sorted views of (name, accessorIndex)
    auto makeList = [](const gltf::Geometry& g) {
        std::vector<std::pair<std::string, std::size_t>> v;
        v.reserve(g.attributes.size());
        for (const auto& at : g.attributes) {
            if (!at.accessorIndex) return std::vector<std::pair<std::string, std::size_t>>{}; // missing info -> treat as unmatched
            v.emplace_back(at.name, *at.accessorIndex);
        }
        std::sort(v.begin(), v.end(), [](auto& l, auto& r){
            if (l.first != r.first) return l.first < r.first;
            return l.second < r.second;
        });
        return v;
    };

    auto la = makeList(a);
    auto lb = makeList(b);
    if (la.empty() || lb.empty()) return false; // cannot dedup without full identity
    return la == lb;
}

static bool Mat4Equal(const glm::mat4& a, const glm::mat4& b) {
    return a == b; // exact compare; values come directly from glTF/doubles cast to float
}

static bool IsIdentity(const glm::mat4& m) {
    return Mat4Equal(m, glm::mat4(1.0f));
}

static bool TRSEqual(const MeshNodeTransform& a, const MeshNodeTransform& b) {
    return a.translation == b.translation && a.rotation == b.rotation && a.scale == b.scale;
}

// internBounds implementation
int32_t Model::internBounds(const BoundingBox& b) {
    // Simple linear dedup; can be improved with hashing if needed
    for (std::size_t i = 0; i < bounds.size(); ++i) {
        const auto& e = bounds[i];
        if (e.aabb.min == b.aabb.min && e.aabb.max == b.aabb.max &&
            e.obb.center == b.obb.center && e.obb.axisX == b.obb.axisX && e.obb.axisY == b.obb.axisY && e.obb.axisZ == b.obb.axisZ && e.obb.halfSize == b.obb.halfSize &&
            e.sphere.center == b.sphere.center && e.sphere.radius == b.sphere.radius) {
            return static_cast<int32_t>(i);
        }
    }
    bounds.push_back(b);
    return static_cast<int32_t>(bounds.size() - 1);
}

int32_t Model::internTRS(const MeshNodeTransform& t) {
    for (std::size_t i = 0; i < trsPool.size(); ++i) {
        if (TRSEqual(trsPool[i], t)) return static_cast<int32_t>(i);
    }
    trsPool.push_back(t);
    return static_cast<int32_t>(trsPool.size() - 1);
}

int32_t Model::internMatrix(const MatrixEntry& m) {
    for (std::size_t i = 0; i < matrixPool.size(); ++i) {
        if (Mat4Equal(matrixPool[i].local, m.local) && Mat4Equal(matrixPool[i].world, m.world)) return static_cast<int32_t>(i);
    }
    matrixPool.push_back(m);
    return static_cast<int32_t>(matrixPool.size() - 1);
}

Model ConvertFromGLTF(const gltf::Model& src) {
    Model dst;
    dst.gltf_source = src.source;

    // materials
    dst.materials.reserve(src.materials.size());
    for (const auto& m : src.materials) {
        pure::Material pm; pm.name = m.name; dst.materials.push_back(std::move(pm));
    }

    // scenes with aabb (scene OBB filled later after nodes computed)
    dst.scenes.reserve(src.scenes.size());
    for (const auto& s : src.scenes) {
        pure::Scene ps; ps.name = s.name;
        // convert root node indices to int32
        ps.nodes.reserve(s.nodes.size());
        for (auto ni : s.nodes) ps.nodes.push_back(static_cast<int32_t>(ni));
        // store provided scene AABB into bounds pool (OBB and sphere will be computed later)
        BoundingBox bb; bb.aabb = s.worldAABB; bb.obb.reset(); bb.sphere.reset();
        ps.boundsIndex = dst.internBounds(bb);
        dst.scenes.push_back(std::move(ps));
    }

    // mesh_nodes (nodes) copy + transform pools
    dst.mesh_nodes.reserve(src.nodes.size());
    for (const auto& n : src.nodes) {
        pure::MeshNode pn;
        pn.name = n.name;
        // convert children indices
        pn.children.reserve(n.children.size());
        for (auto c : n.children) pn.children.push_back(static_cast<int32_t>(c));

        // Fill matrix entry
        MatrixEntry me;
        me.local = glm::mat4(n.localMatrix());
        me.world = glm::mat4(n.worldMatrix);
        const bool isIdentityMatrix = IsIdentity(me.local) && IsIdentity(me.world);
        if (!isIdentityMatrix) {
            const int32_t midx = dst.internMatrix(me);
            pn.matrixIndexPlusOne = midx + 1;
        } else {
            pn.matrixIndexPlusOne = 0; // identity
        }

        // Fill TRS entry only if source used TRS and it's non-identity
        if (!n.hasMatrix) {
            MeshNodeTransform tf;
            tf.translation = glm::vec3(n.translation);
            tf.rotation = glm::quat(n.rotation);
            tf.scale = glm::vec3(n.scale);
            const bool isIdentityTRS = TRSEqual(tf, MeshNodeTransform{});
            if (!isIdentityTRS) {
                const int32_t tidx = dst.internTRS(tf);
                pn.trsIndexPlusOne = tidx + 1;
            } else {
                pn.trsIndexPlusOne = 0;
            }
        } else {
            pn.trsIndexPlusOne = 0; // came from matrix path
        }

        pn.boundsIndex = kInvalidBoundsIndex; // will be filled later
        dst.mesh_nodes.push_back(std::move(pn));
    }

    // Build unique geometry set by accessor identity
    const auto& prims = src.primitives;
    std::vector<int32_t> uniqueRepGeomPrimIdx; // representative primitive index per unique geometry
    std::vector<int32_t> geomIndexOfPrim(static_cast<int32_t>(prims.size()), static_cast<int32_t>(-1));

    for (int32_t i = 0; i < static_cast<int32_t>(prims.size()); ++i) {
        const auto& g = prims[static_cast<std::size_t>(i)].geometry;
        int32_t found = static_cast<int32_t>(-1);
        for (int32_t u = 0; u < static_cast<int32_t>(uniqueRepGeomPrimIdx.size()); ++u) {
            if (SameGeometryByAccessorId(g, prims[static_cast<std::size_t>(uniqueRepGeomPrimIdx[static_cast<std::size_t>(u)])].geometry)) { found = u; break; }
        }
        if (found == static_cast<int32_t>(-1)) {
            uniqueRepGeomPrimIdx.push_back(i);
            geomIndexOfPrim[static_cast<std::size_t>(i)] = static_cast<int32_t>(uniqueRepGeomPrimIdx.size() - 1);
        } else {
            geomIndexOfPrim[static_cast<std::size_t>(i)] = found;
        }
    }

    // geometry (pure geometry) metadata + binary for unique ones
    dst.geometry.reserve(uniqueRepGeomPrimIdx.size());

    for (int32_t u = 0; u < static_cast<int32_t>(uniqueRepGeomPrimIdx.size()); ++u) {
        const int32_t i = uniqueRepGeomPrimIdx[static_cast<std::size_t>(u)];
        const auto& g = prims[static_cast<std::size_t>(i)].geometry;
        pure::Geometry pg;
        pg.mode = g.mode;
        pg.attributes.reserve(g.attributes.size());
        for (std::size_t ai = 0; ai < g.attributes.size(); ++ai) {
            const auto& a = g.attributes[ai];
            pure::GeometryAttribute ga;
            ga.id = static_cast<int64_t>(ai);
            ga.name = a.name;
            ga.count = a.count;
            ga.componentType = a.componentType;
            ga.type = a.type;
            ga.format = a.format;
            ga.data = a.data; // copy binary
            pg.attributes.push_back(std::move(ga));
        }

        // Compute and store local-space bounds and positions (decode ONCE per unique geometry)
        ComputeGeometryBoundsFromGLTF(dst, g, pg);

        if (g.indices) {
            pg.indicesData = *g.indices; // copy raw indices buffer
        }
        if (g.indexCount && g.indexComponentType) {
            pg.indices = pure::GeometryIndicesMeta{ *g.indexCount, *g.indexComponentType };

            pg.indices->indexType = g.indexType;
        }
        dst.geometry.push_back(std::move(pg));
    }

    // Global SubMeshes list: one-to-one with glTF primitives (geometry + material)
    dst.subMeshes.reserve(prims.size());
    for (int32_t i = 0; i < static_cast<int32_t>(prims.size()); ++i) {
        const auto& p = prims[static_cast<std::size_t>(i)];
        pure::SubMesh sm;
        sm.geometry = geomIndexOfPrim[static_cast<std::size_t>(i)];
        sm.material = p.material ? std::optional<int32_t>(static_cast<int32_t>(*p.material)) : std::optional<int32_t>{};
        dst.subMeshes.push_back(std::move(sm));
    }

    // Attach per-node SubMeshes indices (each equals a glTF primitive under the node's glTF mesh)
    for (int32_t ni = 0; ni < static_cast<int32_t>(src.nodes.size()); ++ni) {
        const auto& node = src.nodes[static_cast<std::size_t>(ni)];
        auto& pn = dst.mesh_nodes[static_cast<std::size_t>(ni)];
        if (node.mesh) {
            const auto& mesh = src.meshes[static_cast<std::size_t>(*node.mesh)];
            pn.subMeshes.reserve(mesh.primitives.size());
            for (auto primIndex : mesh.primitives) {
                pn.subMeshes.push_back(static_cast<int32_t>(primIndex)); // index into global subMeshes as int32_t
            }
        }
    }

    // Compute per-node world-space bounds from attached subMeshes and node world_matrix
    ComputeAllMeshNodeBounds(dst);

    // Compute per-scene OBB and sphere from all world-space points under its nodes
    ComputeAllSceneBounds(dst);

    return dst;
}

} // namespace pure
