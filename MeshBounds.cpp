#include "MeshBounds.h"

#include <vector>

namespace pure {

// local helper: decode POSITION to double3
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

void ComputeGeometryBoundsFromGLTF(pure::Model& model, const gltf::Geometry& srcGeom, pure::Geometry& dstGeom) {
    BoundingBox bb; // local aggregate
    bb.aabb = srcGeom.localAABB; // AABB comes from glTF primitive local AABB

    // Decode positions once and compute OBB and sphere
    std::vector<glm::dvec3> pos;
    if (TryDecodePositionsAsDVec3(srcGeom, pos) && !pos.empty()) {
        dstGeom.positions = pos; // cache decoded local-space positions
        bb.obb = OBB::fromPointsMinVolume(pos);
    } else {
        dstGeom.positions.reset();
        bb.obb.reset();
    }

    // Compute sphere for geometry (prefer points if available; fallback to AABB)
    if (dstGeom.positions && !dstGeom.positions->empty()) {
        bb.sphere = SphereFromPoints(*dstGeom.positions);
    } else if (!bb.aabb.empty()) {
        bb.sphere = SphereFromAABB(bb.aabb);
    } else {
        bb.sphere.reset();
    }

    dstGeom.boundsIndex = model.internBounds(bb);
}

void ComputeMeshNodeBounds(pure::Model& model, std::size_t nodeIndex) {
    auto& node = model.mesh_nodes[nodeIndex];

    BoundingBox nb; // start empty
    nb.aabb.reset();
    nb.obb.reset();
    nb.sphere.reset();
    if (node.subMeshes.empty()) { node.boundsIndex = model.internBounds(nb); return; }

    const glm::dmat4 world = glm::dmat4(node.world_matrix);

    std::vector<glm::dvec3> worldPoints; worldPoints.reserve(1024);
    for (auto smIndex : node.subMeshes) {
        const auto& sm = model.subMeshes[smIndex];
        if (sm.geometry == static_cast<std::size_t>(-1)) continue;
        const auto& g = model.geometry[sm.geometry];
        if (g.boundsIndex != kInvalidBoundsIndex) {
            const BoundingBox& gb = model.bounds[g.boundsIndex];
            if (!gb.aabb.empty()) {
                nb.aabb.merge(gb.aabb.transformed(world));
            }
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
        nb.obb = OBB::fromPointsMinVolume(worldPoints);
    }

    // Compute sphere for node (prefer points if available; fallback to AABB)
    if (!worldPoints.empty()) {
        nb.sphere = SphereFromPoints(worldPoints);
    } else if (!nb.aabb.empty()) {
        nb.sphere = SphereFromAABB(nb.aabb);
    } else {
        nb.sphere.reset();
    }

    node.boundsIndex = model.internBounds(nb);
}

void ComputeAllMeshNodeBounds(pure::Model& model) {
    for (std::size_t ni = 0; ni < model.mesh_nodes.size(); ++ni) {
        ComputeMeshNodeBounds(model, ni);
    }
}

void ComputeSceneBounds(pure::Model& model, pure::Scene& scene) {
    BoundingBox sb; sb.obb.reset(); sb.sphere.reset();

    std::vector<glm::dvec3> scenePts; scenePts.reserve(4096);

    // traverse nodes of scene and gather each node's world-space points again
    std::vector<std::size_t> stack(scene.nodes.begin(), scene.nodes.end());
    while (!stack.empty()) {
        auto ni = stack.back(); stack.pop_back();
        const auto& node = model.mesh_nodes[ni];
        const glm::dmat4 world = glm::dmat4(node.world_matrix);

        for (auto smIndex : node.subMeshes) {
            const auto& sm = model.subMeshes[smIndex];
            if (sm.geometry == static_cast<std::size_t>(-1)) continue;
            const auto& g = model.geometry[sm.geometry];
            if (g.positions && !g.positions->empty()) {
                for (const auto& p : *g.positions) {
                    glm::dvec4 hp = world * glm::dvec4(p, 1.0);
                    scenePts.emplace_back(hp.x, hp.y, hp.z);
                }
            }
        }
        for (auto c : node.children) stack.push_back(c);
    }

    if (!scenePts.empty()) sb.obb = OBB::fromPointsMinVolume(scenePts);

    // Compute sphere for scene (prefer points if available; fallback to AABB)
    if (!scenePts.empty()) {
        sb.sphere = SphereFromPoints(scenePts);
    } else if (scene.boundsIndex != kInvalidBoundsIndex) {
        const auto& existing = model.bounds[scene.boundsIndex];
        if (!existing.aabb.empty()) sb.sphere = SphereFromAABB(existing.aabb);
    } else {
        sb.sphere.reset();
    }

    // Keep existing AABB if any (copied from glTF at scene creation)
    if (scene.boundsIndex != kInvalidBoundsIndex) {
        const auto& existing = model.bounds[scene.boundsIndex];
        sb.aabb = existing.aabb;
    }

    scene.boundsIndex = model.internBounds(sb);
}

void ComputeAllSceneBounds(pure::Model& model) {
    for (auto& sc : model.scenes) {
        ComputeSceneBounds(model, sc);
    }
}

} // namespace pure
