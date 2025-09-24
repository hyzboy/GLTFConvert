#include "StaticMesh.h"

#include <algorithm>
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

// internBounds implementation
std::size_t Model::internBounds(const BoundingBox& b) {
    // Simple linear dedup; can be improved with hashing if needed
    for (std::size_t i = 0; i < bounds.size(); ++i) {
        const auto& e = bounds[i];
        if (e.aabb.min == b.aabb.min && e.aabb.max == b.aabb.max &&
            e.obb.center == b.obb.center && e.obb.axisX == b.obb.axisX && e.obb.axisY == b.obb.axisY && e.obb.axisZ == b.obb.axisZ && e.obb.halfSize == b.obb.halfSize &&
            e.sphere.center == b.sphere.center && e.sphere.radius == b.sphere.radius) {
            return i;
        }
    }
    bounds.push_back(b);
    return bounds.size() - 1;
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
        pure::Scene ps; ps.name = s.name; ps.nodes = s.nodes; 
        // store provided scene AABB into bounds pool (OBB and sphere will be computed later)
        BoundingBox bb; bb.aabb = s.worldAABB; bb.obb.reset(); bb.sphere.reset();
        ps.boundsIndex = dst.internBounds(bb);
        dst.scenes.push_back(std::move(ps));
    }

    // mesh_nodes (nodes) copy + world matrices
    dst.mesh_nodes.reserve(src.nodes.size());
    for (const auto& n : src.nodes) {
        pure::MeshNode pn;
        pn.name = n.name;
        pn.children = n.children;
        if (n.hasMatrix) {
            pn.matrix = n.matrix;
        } else {
            // restore TRS into float-based transform
            MeshNodeTransform tf;
            tf.translation = glm::vec3(n.translation);
            tf.rotation = glm::quat(n.rotation);
            tf.scale = glm::vec3(n.scale);
            pn.transform = tf;
        }
        // convert double-precision world matrix to float for storage
        pn.world_matrix = glm::mat4(n.worldMatrix);
        pn.boundsIndex = kInvalidBoundsIndex; // will be filled later
        dst.mesh_nodes.push_back(std::move(pn));
    }

    // Build unique geometry set by accessor identity
    const auto& prims = src.primitives;
    std::vector<std::size_t> uniqueRepGeomPrimIdx; // representative primitive index per unique geometry
    std::vector<std::size_t> geomIndexOfPrim(prims.size(), static_cast<std::size_t>(-1));

    for (std::size_t i = 0; i < prims.size(); ++i) {
        const auto& g = prims[i].geometry;
        std::size_t found = static_cast<std::size_t>(-1);
        for (std::size_t u = 0; u < uniqueRepGeomPrimIdx.size(); ++u) {
            if (SameGeometryByAccessorId(g, prims[uniqueRepGeomPrimIdx[u]].geometry)) { found = u; break; }
        }
        if (found == static_cast<std::size_t>(-1)) {
            uniqueRepGeomPrimIdx.push_back(i);
            geomIndexOfPrim[i] = uniqueRepGeomPrimIdx.size() - 1;
        } else {
            geomIndexOfPrim[i] = found;
        }
    }

    // geometry (pure geometry) metadata + binary for unique ones
    dst.geometry.reserve(uniqueRepGeomPrimIdx.size());

    for (std::size_t u = 0; u < uniqueRepGeomPrimIdx.size(); ++u) {
        const std::size_t i = uniqueRepGeomPrimIdx[u];
        const auto& g = prims[i].geometry;
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
        }
        dst.geometry.push_back(std::move(pg));
    }

    // Global SubMeshes list: one-to-one with glTF primitives (geometry + material)
    dst.subMeshes.reserve(prims.size());
    for (std::size_t i = 0; i < prims.size(); ++i) {
        const auto& p = prims[i];
        pure::SubMesh sm;
        sm.geometry = geomIndexOfPrim[i];
        sm.material = p.material;
        dst.subMeshes.push_back(std::move(sm));
    }

    // Attach per-node SubMeshes indices (each equals a glTF primitive under the node's glTF mesh)
    for (std::size_t ni = 0; ni < src.nodes.size(); ++ni) {
        const auto& node = src.nodes[ni];
        auto& pn = dst.mesh_nodes[ni];
        if (node.mesh) {
            const auto& mesh = src.meshes[*node.mesh];
            pn.subMeshes.reserve(mesh.primitives.size());
            for (auto primIndex : mesh.primitives) {
                pn.subMeshes.push_back(static_cast<std::size_t>(primIndex)); // index into global subMeshes
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
