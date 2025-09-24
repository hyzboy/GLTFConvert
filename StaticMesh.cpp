#include "StaticMesh.h"

#include <algorithm>

namespace pure {

// Helper: sphere from AABB (center = mid, radius = half diagonal)
static BoundingSphere SphereFromAABB(const AABB& a) {
    BoundingSphere s; s.reset();
    if (!a.empty()) {
        s.center = (a.min + a.max) * 0.5;
        s.radius = glm::length(a.max - s.center);
    }
    return s;
}

// Helper: sphere from points (center = average, radius = max distance to center)
static BoundingSphere SphereFromPoints(const std::vector<glm::dvec3>& pts) {
    BoundingSphere s; s.reset();
    if (pts.empty()) return s;
    glm::dvec3 c(0.0);
    for (const auto& p : pts) c += p;
    c /= static_cast<double>(pts.size());
    double r = 0.0;
    for (const auto& p : pts) r = std::max(r, glm::length(p - c));
    s.center = c;
    s.radius = r;
    return s;
}

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

static bool TryDecodePositionsAsDVec3(const gltf::Geometry& g, std::vector<glm::dvec3>& out) {
    out.clear();
    for (const auto& a : g.attributes) {
        if (a.name == "POSITION" && a.componentType == "FLOAT" && (a.type == "VEC3" || a.type == "VEC4")) {
            // assume tightly packed float3/float4
            const std::byte* ptr = a.data.data();
            const std::size_t elementCount = a.count;
            const std::size_t stride = (a.type == "VEC3") ? sizeof(float) * 3 : sizeof(float) * 4;
            if (a.data.size() < elementCount * stride) return false;
            out.resize(elementCount);
            for (std::size_t i = 0; i < elementCount; ++i) {
                const float* f = reinterpret_cast<const float*>(ptr + i * stride);
                out[i] = glm::dvec3(f[0], f[1], f[2]);
            }
            return true;
        }
    }
    return false;
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
        pure::Scene ps; ps.name = s.name; ps.nodes = s.nodes; ps.bounds.aabb = s.worldAABB; /*ps.bounds.obb stays empty*/ dst.scenes.push_back(std::move(ps));
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
        pg.bounds.aabb = g.localAABB;

        // Compute and store local-space positions (double) if available, and compute OBB (decode ONCE per unique geometry)
        {
            std::vector<glm::dvec3> pos;
            if (TryDecodePositionsAsDVec3(g, pos) && !pos.empty()) {
                pg.positions = pos; // cache in Geometry
                pg.bounds.obb = OBB::fromPointsMinVolume(pos);
            } else {
                pg.positions.reset();
                pg.bounds.obb.reset();
            }
        }

        // Compute sphere for geometry (prefer points if available; fallback to AABB)
        if (pg.positions && !pg.positions->empty()) {
            pg.bounds.sphere = SphereFromPoints(*pg.positions);
        } else if (!pg.bounds.aabb.empty()) {
            pg.bounds.sphere = SphereFromAABB(pg.bounds.aabb);
        } else {
            pg.bounds.sphere.reset();
        }

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

    // Compute per-node world-space AABB and OBB from attached subMeshes and node world_matrix
    for (std::size_t ni = 0; ni < dst.mesh_nodes.size(); ++ni) {
        auto& pn = dst.mesh_nodes[ni];
        pn.bounds.aabb.reset();
        pn.bounds.obb.reset();
        pn.bounds.sphere.reset();
        if (pn.subMeshes.empty()) continue;
        const glm::dmat4 world = glm::dmat4(pn.world_matrix);

        std::vector<glm::dvec3> worldPoints; worldPoints.reserve(1024);
        for (auto smIndex : pn.subMeshes) {
            const auto& sm = dst.subMeshes[smIndex];
            if (sm.geometry == static_cast<std::size_t>(-1)) continue;
            const auto& g = dst.geometry[sm.geometry];
            if (!g.bounds.aabb.empty()) {
                pn.bounds.aabb.merge(g.bounds.aabb.transformed(world));
            }
            // collect transformed positions using cached positions
            if (g.positions && !g.positions->empty()) {
                for (const auto& p : *g.positions) {
                    glm::dvec4 hp = world * glm::dvec4(p, 1.0);
                    worldPoints.emplace_back(hp.x, hp.y, hp.z);
                }
            }
        }
        if (!worldPoints.empty()) {
            pn.bounds.obb = OBB::fromPointsMinVolume(worldPoints);
        }
        // Compute sphere for node (prefer points if available; fallback to AABB)
        if (!worldPoints.empty()) {
            pn.bounds.sphere = SphereFromPoints(worldPoints);
        } else if (!pn.bounds.aabb.empty()) {
            pn.bounds.sphere = SphereFromAABB(pn.bounds.aabb);
        } else {
            pn.bounds.sphere.reset();
        }
    }

    // Compute per-scene OBB from all world-space points under its nodes
    for (std::size_t si = 0; si < dst.scenes.size(); ++si) {
        auto& sc = dst.scenes[si];
        sc.bounds.obb.reset();
        sc.bounds.sphere.reset();
        std::vector<glm::dvec3> scenePts; scenePts.reserve(4096);

        // traverse nodes of scene and gather each node's world-space points again
        std::vector<std::size_t> stack(sc.nodes.begin(), sc.nodes.end());
        while (!stack.empty()) {
            auto ni = stack.back(); stack.pop_back();
            const auto& node = src.nodes[ni];
            if (node.mesh) {
                const auto& mesh = src.meshes[*node.mesh];
                const glm::dmat4 world = node.worldMatrix;
                for (auto primIndex : mesh.primitives) {
                    const std::size_t gi = geomIndexOfPrim[primIndex];
                    if (gi != static_cast<std::size_t>(-1)) {
                        const auto& g = dst.geometry[gi];
                        if (g.positions && !g.positions->empty()) {
                            for (const auto& p : *g.positions) {
                                glm::dvec4 hp = world * glm::dvec4(p, 1.0);
                                scenePts.emplace_back(hp.x, hp.y, hp.z);
                            }
                        }
                    }
                }
            }
            for (auto c : node.children) stack.push_back(c);
        }
        if (!scenePts.empty()) sc.bounds.obb = OBB::fromPointsMinVolume(scenePts);

        // Compute sphere for scene (prefer points if available; fallback to AABB)
        if (!scenePts.empty()) {
            sc.bounds.sphere = SphereFromPoints(scenePts);
        } else if (!sc.bounds.aabb.empty()) {
            sc.bounds.sphere = SphereFromAABB(sc.bounds.aabb);
        } else {
            sc.bounds.sphere.reset();
        }
    }

    return dst;
}

} // namespace pure
