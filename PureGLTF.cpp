#include "PureGLTF.h"

namespace puregltf {

void Model::computeWorldMatrices() {
    // initialize world matrices with identity
    for (auto &n : nodes) n.worldMatrix = glm::dmat4(1.0);

    // DFS from scene roots because nodes can be shared across scenes; we compute per-scene pass but keep last
    std::vector<std::size_t> stack;
    for (const auto &sc : scenes) {
        stack.clear();
        for (auto r : sc.nodes) stack.push_back(r);
        while (!stack.empty()) {
            auto idx = stack.back(); stack.pop_back();
            auto &node = nodes[idx];
            // if parent known? We don't track parents; process children using current world
            glm::dmat4 parentWorld = node.worldMatrix; // if already set from previous pass
            glm::dmat4 wm = parentWorld == glm::dmat4(1.0) ? node.localMatrix() : parentWorld * node.localMatrix();
            node.worldMatrix = wm;
            for (auto c : node.children) {
                // set child's world as current*childLocal (will be updated when visited)
                nodes[c].worldMatrix = wm * nodes[c].localMatrix();
                stack.push_back(c);
            }
        }
    }
}

void Model::computeSceneAABBs() {
    for (auto &sc : scenes) sc.worldAABB.reset();

    // helper lambda to expand scene by a node
    auto expandByNode = [&](Scene &scene, const Node &node){
        if (!node.mesh) return;
        const auto &mesh = meshes[*node.mesh];
        for (std::size_t pidx : mesh.primitives) {
            const auto &prim = primitives[pidx];
            if (!prim.localAABB.empty()) {
                scene.worldAABB.merge(prim.localAABB.transformed(node.worldMatrix));
            }
        }
    };

    // traverse nodes per scene
    std::vector<std::size_t> stack;
    for (auto si = 0u; si < scenes.size(); ++si) {
        auto &scene = scenes[si];
        stack.clear();
        for (auto r : scene.nodes) stack.push_back(r);
        while (!stack.empty()) {
            auto idx = stack.back(); stack.pop_back();
            const auto &node = nodes[idx];
            expandByNode(scene, node);
            for (auto c : node.children) stack.push_back(c);
        }
    }
}

void Model::convertToZUp() {
    // +90 degrees around X axis rotates Y-up to Z-up
    const double angle = glm::radians(90.0);
    const glm::dvec3 axis(1.0, 0.0, 0.0);

    const glm::dmat4 R = glm::rotate(glm::dmat4(1.0), angle, axis);
    const glm::dmat4 Rinv = glm::transpose(R); // rotation matrix inverse == transpose

    const glm::dquat rq = glm::angleAxis(angle, axis);
    const glm::dquat rqInv = glm::inverse(rq);

    for (auto &n : nodes) {
        if (n.hasMatrix) {
            n.matrix = R * n.matrix * Rinv;
        } else {
            // translation as a point (rotation only so w doesn't matter)
            glm::dvec4 t4(n.translation, 1.0);
            t4 = R * t4;
            n.translation = glm::dvec3(t4);

            // rotate orientation via conjugation to change basis
            n.rotation = glm::normalize(rq * n.rotation * rqInv);
            // scale stays component-wise the same under a pure basis change
        }
    }

    // Recompute derived data
    computeWorldMatrices();
    computeSceneAABBs();
}

} // namespace puregltf
