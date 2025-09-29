#include "Exporter.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <cctype>
#include <cstdint>

#include "StaticMesh.h"
#include "SceneBinary.h"

using nlohmann::json;

namespace exporters {

static std::string stem_noext(const std::filesystem::path& p) {
    return p.stem().string();
}

// Helper to write bounds as a binary file (Bounds.bin)
static bool WriteBoundsBinaryTo(const std::vector<BoundingBox>& bounds, const std::filesystem::path& dir)
{
    std::filesystem::path binPath = dir / "Bounds.bin";
    std::ofstream ofs(binPath, std::ios::binary);
    if (!ofs) {
        std::cerr << "[Export] Cannot open bounds binary: " << binPath << "\n";
        return false;
    }

    auto writeVec3 = [&](const glm::dvec3& v) {
        double d[3] = { v.x, v.y, v.z };
        ofs.write(reinterpret_cast<const char*>(d), sizeof(d));
    };
    auto writeDouble = [&](double d) {
        ofs.write(reinterpret_cast<const char*>(&d), sizeof(d));
    };

    // Fixed layout per entry: AABB(min.xyz, max.xyz) + OBB(center.xyz, axisX.xyz, axisY.xyz, axisZ.xyz, halfSize.xyz) + Sphere(center.xyz, radius)
    for (const auto& b : bounds) {
        // AABB
        writeVec3(b.aabb.min);
        writeVec3(b.aabb.max);
        // OBB
        writeVec3(b.obb.center);
        writeVec3(b.obb.axisX);
        writeVec3(b.obb.axisY);
        writeVec3(b.obb.axisZ);
        writeVec3(b.obb.halfSize);
        // Sphere
        writeVec3(b.sphere.center);
        writeDouble(b.sphere.radius);
    }

    ofs.close();
    std::cout << "[Export] Saved: " << binPath << "\n";
    return true;
}

// Helper to write all bounds as a single binary file (Bounds.bin) in targetDir (legacy/global)
static bool WriteAllBoundsBinary(const pure::Model& sm, const std::filesystem::path& dir)
{
    return WriteBoundsBinaryTo(sm.bounds, dir);
}

// Sanitize a scene name to safe folder/file name
static std::string SanitizeName(const std::string& in)
{
    if (in.empty()) return std::string();
    std::string out; out.reserve(in.size());
    for (char c : in) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c=='_' || c=='-' ) out.push_back(c);
        else out.push_back('_');
    }
    return out;
}

bool ExportPureModel(const gltf::Model& model, const std::filesystem::path& outDir) {
    // Convert source glTF model into pure static-mesh model first
    pure::Model sm = pure::ConvertFromGLTF(model);

    std::filesystem::path baseDir = outDir.empty() ? std::filesystem::path(sm.gltf_source).parent_path() : outDir;
    std::error_code ec;
    std::filesystem::create_directories(baseDir, ec);

    const std::string baseName = stem_noext(sm.gltf_source);
    std::filesystem::path targetDir = baseDir / (baseName + ".StaticMesh");
    std::filesystem::create_directories(targetDir, ec);

    json root = json::object();
    root["gltf_source"] = sm.gltf_source;

    // materials (names only for now)
    json materials = json::array();
    for (const auto& mtl : sm.materials) {
        json mj = json::object();
        if (!mtl.name.empty()) mj["name"] = mtl.name;
        materials.push_back(std::move(mj));
    }
    root["materials"] = std::move(materials);

    // Write bounds binary once and only store count in JSON (global)
    if (!WriteAllBoundsBinary(sm, targetDir)) {
        std::cerr << "[Export] Failed to write bounds binary." << "\n";
    }
    root["bounds"] = static_cast<int64_t>(sm.bounds.size());

    // scenes (global listing with original indices, preserved for legacy)
    json scenes = json::array();
    for (const auto& sc : sm.scenes) {
        json s = json::object();
        if (!sc.name.empty()) s["name"] = sc.name;
        json ns = json::array();
        for (auto n : sc.nodes) ns.push_back(static_cast<int64_t>(n));
        s["nodes"] = std::move(ns);

        // reference bounds by index
        s["bounds"] = (sc.boundsIndex == pure::kInvalidBoundsIndex) ? json(nullptr) : json(static_cast<int64_t>(sc.boundsIndex));

        scenes.push_back(std::move(s));
    }
    root["scenes"] = std::move(scenes);

    // mesh_nodes and transform indices (subMeshes are already attached in pure model)
    json meshNodes = json::array();
    for (const auto& n : sm.mesh_nodes) {
        json j = json::object();
        if (!n.name.empty()) j["name"] = n.name;
        json ch = json::array();
        for (auto c : n.children) ch.push_back(static_cast<int64_t>(c));
        j["children"] = std::move(ch);

        // indices
        j["matrix"] = static_cast<int64_t>(n.matrixIndexPlusOne);
        j["trs"] = static_cast<int64_t>(n.trsIndexPlusOne);

        // per-node subMeshes (indices into global subMeshes)
        json sms = json::array();
        for (auto primIndex : n.subMeshes) sms.push_back(static_cast<int64_t>(primIndex));
        j["subMeshes"] = std::move(sms);

        // reference bounds by index
        j["bounds"] = (n.boundsIndex == pure::kInvalidBoundsIndex) ? json(nullptr) : json(static_cast<int64_t>(n.boundsIndex));

        meshNodes.push_back(std::move(j));
    }
    root["mesh_nodes"] = std::move(meshNodes);

    // Export matrix pool as a binary file (local/world pairs)
    {
        std::filesystem::path binPath = targetDir / "Matrices.bin";
        std::ofstream ofs(binPath, std::ios::binary);
        if (ofs) {
            for (const auto& m : sm.matrixPool) {
                ofs.write(reinterpret_cast<const char*>(&m.local), sizeof(glm::mat4));
                ofs.write(reinterpret_cast<const char*>(&m.world), sizeof(glm::mat4));
            }
            ofs.close();
            std::cout << "[Export] Saved: " << binPath << "\n";
        }
        // store count in main JSON
        root["matrices"] = static_cast<int64_t>(sm.matrixPool.size());
    }

    // Export TRS pool as a compact binary file: [tx ty tz rx ry rz rw sx sy sz] floats per entry
    {
        std::filesystem::path binPath = targetDir / "TRS.bin";
        std::ofstream ofs(binPath, std::ios::binary);
        if (ofs) {
            for (const auto& t : sm.trsPool) {
                const float tvals[3] = { t.translation.x, t.translation.y, t.translation.z };
                const float rvals[4] = { t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w }; // xyzw
                const float svals[3] = { t.scale.x, t.scale.y, t.scale.z };
                ofs.write(reinterpret_cast<const char*>(tvals), sizeof(tvals));
                ofs.write(reinterpret_cast<const char*>(rvals), sizeof(rvals));
                ofs.write(reinterpret_cast<const char*>(svals), sizeof(svals));
            }
            ofs.close();
            std::cout << "[Export] Saved: " << binPath << "\n";
        }
        // store count in main JSON
        root["trs_count"] = static_cast<int64_t>(sm.trsPool.size());
    }

    // Only export consolidated .geometry files; drop per-attribute/indices files and geometry JSON
    for (std::size_t u = 0; u < sm.geometry.size(); ++u) {
        const auto& g = sm.geometry[u];
        std::filesystem::path binName = stem_noext(sm.gltf_source) + "." + std::to_string(u) + ".geometry";
        std::filesystem::path binPath = targetDir / binName;

        const BoundingBox &geo_bounds = (g.boundsIndex != pure::kInvalidBoundsIndex) ? sm.bounds[g.boundsIndex] : BoundingBox{};

        if (!pure::SaveGeometry(g, geo_bounds, binPath.string()))
        {
            std::cerr << "[Export] Failed to write geometry binary: " << binPath << "\n";
        }
    }

    // Global SubMeshes list (kept; references geometry indices that map to <index>.geometry files)
    json subMeshes = json::array();
    for (const auto& p : sm.subMeshes) {
        json sj = json::object();
        sj["geometry"] = static_cast<int64_t>(p.geometry);
        if (p.material) sj["material"] = static_cast<int64_t>(*p.material); else sj["material"] = nullptr;
        subMeshes.push_back(std::move(sj));
    }
    root["subMeshes"] = std::move(subMeshes);

    std::filesystem::path jsonPath = targetDir / ("StaticMesh.json");
    std::ofstream jsonOut(jsonPath, std::ios::binary);
    if (!jsonOut) {
        std::cerr << "[Export] Cannot open: " << jsonPath << "\n";
        return false;
    }

    jsonOut << root.dump(4);
    jsonOut.close();
    std::cout << "[Export] Saved: " << jsonPath << "\n";

    // Per-scene independent exports with remapped indices for nodes/subMeshes/bounds/matrices/trs
    for (std::size_t si = 0; si < sm.scenes.size(); ++si) {
        const auto& sc = sm.scenes[si];

        // 1) Collect reachable nodes from scene roots
        std::vector<uint8_t> nodeUsed(sm.mesh_nodes.size(), 0);
        std::vector<std::size_t> nodeOrder; nodeOrder.reserve(sm.mesh_nodes.size());
        std::vector<std::size_t> stack(sc.nodes.begin(), sc.nodes.end());
        while (!stack.empty()) {
            auto ni = stack.back(); stack.pop_back();
            if (ni >= sm.mesh_nodes.size()) continue;
            if (nodeUsed[ni]) continue;
            nodeUsed[ni] = 1;
            nodeOrder.push_back(ni);
            for (auto c : sm.mesh_nodes[ni].children) {
                stack.push_back(c);
            }
        }
        // Stable order by original index to have deterministic mapping
        std::sort(nodeOrder.begin(), nodeOrder.end());

        // 2) Build remap tables
        std::unordered_map<std::size_t, std::size_t> nodeRemap; nodeRemap.reserve(nodeOrder.size());
        for (std::size_t i = 0; i < nodeOrder.size(); ++i) nodeRemap[nodeOrder[i]] = i;

        std::unordered_map<std::size_t, std::size_t> subMeshRemap; // old -> new
        std::unordered_map<std::size_t, std::size_t> matrixRemap;  // old (0-based) -> new (0-based)
        std::unordered_map<std::size_t, std::size_t> trsRemap;     // old (0-based) -> new (0-based)
        std::unordered_map<std::size_t, std::size_t> boundsRemap;  // old -> new

        // Collect used submeshes, matrices, trs, bounds from nodes
        for (auto oldNi : nodeOrder) {
            const auto& n = sm.mesh_nodes[oldNi];
            // subMeshes
            for (auto smi : n.subMeshes) subMeshRemap.try_emplace(smi, subMeshRemap.size());
            // matrices
            if (n.matrixIndexPlusOne != 0) {
                const auto oldM = n.matrixIndexPlusOne - 1;
                if (matrixRemap.find(oldM) == matrixRemap.end()) {
                    matrixRemap.emplace(oldM, matrixRemap.size());
                }
            }
            // trs
            if (n.trsIndexPlusOne != 0) {
                const auto oldT = n.trsIndexPlusOne - 1;
                if (trsRemap.find(oldT) == trsRemap.end()) {
                    trsRemap.emplace(oldT, trsRemap.size());
                }
            }
            // bounds
            if (n.boundsIndex != pure::kInvalidBoundsIndex) {
                boundsRemap.try_emplace(n.boundsIndex, boundsRemap.size());
            }
        }
        // include scene bounds
        if (sc.boundsIndex != pure::kInvalidBoundsIndex) boundsRemap.try_emplace(sc.boundsIndex, boundsRemap.size());

        // 3) Build local pools by remap order (ensure indices 0..N-1 contiguous)
        // subMeshes
        std::vector<pure::SubMesh> localSubMeshes(subMeshRemap.size());
        for (const auto& [oldIdx, newIdx] : subMeshRemap) {
            localSubMeshes[newIdx] = sm.subMeshes[oldIdx];
        }
        // matrices
        std::vector<pure::MatrixEntry> localMatrices(matrixRemap.size());
        for (const auto& [oldIdx, newIdx] : matrixRemap) {
            localMatrices[newIdx] = sm.matrixPool[oldIdx];
        }
        // trs
        std::vector<pure::MeshNodeTransform> localTRS(trsRemap.size());
        for (const auto& [oldIdx, newIdx] : trsRemap) {
            localTRS[newIdx] = sm.trsPool[oldIdx];
        }
        // bounds
        std::vector<BoundingBox> localBounds(boundsRemap.size());
        for (const auto& [oldIdx, newIdx] : boundsRemap) {
            localBounds[newIdx] = sm.bounds[oldIdx];
        }

        // 4) Build local nodes with remapped indices
        std::vector<pure::MeshNode> localNodes; localNodes.reserve(nodeOrder.size());
        for (auto oldNi : nodeOrder) {
            const auto& on = sm.mesh_nodes[oldNi];
            pure::MeshNode nn = on; // copy
            // remap children
            std::vector<std::size_t> newChildren; newChildren.reserve(on.children.size());
            for (auto oc : on.children) {
                auto it = nodeRemap.find(oc);
                if (it != nodeRemap.end()) newChildren.push_back(it->second);
            }
            nn.children = std::move(newChildren);
            // remap subMeshes
            std::vector<std::size_t> newSM; newSM.reserve(on.subMeshes.size());
            for (auto osm : on.subMeshes) newSM.push_back(subMeshRemap[osm]);
            nn.subMeshes = std::move(newSM);
            // remap matrix/trs (plus one semantics)
            if (nn.matrixIndexPlusOne != 0) {
                const auto oldM = nn.matrixIndexPlusOne - 1;
                nn.matrixIndexPlusOne = matrixRemap[oldM] + 1;
            }
            if (nn.trsIndexPlusOne != 0) {
                const auto oldT = nn.trsIndexPlusOne - 1;
                nn.trsIndexPlusOne = trsRemap[oldT] + 1;
            }
            // remap bounds
            if (nn.boundsIndex != pure::kInvalidBoundsIndex) {
                nn.boundsIndex = boundsRemap[nn.boundsIndex];
            }
            localNodes.push_back(std::move(nn));
        }

        // 5) Remap scene roots and bounds
        json sceneJson = json::object();
        if (!sc.name.empty()) sceneJson["name"] = sc.name;
        json roots = json::array();
        std::vector<std::size_t> sceneRootIndices; sceneRootIndices.reserve(sc.nodes.size());
        for (auto r : sc.nodes) {
            auto it = nodeRemap.find(r);
            if (it != nodeRemap.end()) {
                roots.push_back(static_cast<int64_t>(it->second));
                sceneRootIndices.push_back(it->second);
            }
        }
        sceneJson["nodes"] = std::move(roots);
        if (sc.boundsIndex != pure::kInvalidBoundsIndex) {
            sceneJson["bounds"] = static_cast<int64_t>(boundsRemap[sc.boundsIndex]);
        } else {
            sceneJson["bounds"] = nullptr;
        }

        // 6) Write scene folder outputs
        std::string sceneFolderName;
        {
            std::string sane = SanitizeName(sc.name);
            if (sane.empty()) sceneFolderName = "Scene_" + std::to_string(si);
            else sceneFolderName = std::to_string(si) + "_" + sane;
        }
        std::filesystem::path sceneDir = targetDir / sceneFolderName;
        std::filesystem::create_directories(sceneDir, ec);

        // 6.1) Matrices.bin (scene-local)
        {
            std::filesystem::path binPath = sceneDir / "Matrices.bin";
            std::ofstream ofs(binPath, std::ios::binary);
            if (ofs) {
                for (const auto& m : localMatrices) {
                    ofs.write(reinterpret_cast<const char*>(&m.local), sizeof(glm::mat4));
                    ofs.write(reinterpret_cast<const char*>(&m.world), sizeof(glm::mat4));
                }
                ofs.close();
                std::cout << "[Export] Saved: " << binPath << "\n";
            }
        }
        // 6.2) TRS.bin (scene-local)
        {
            std::filesystem::path binPath = sceneDir / "TRS.bin";
            std::ofstream ofs(binPath, std::ios::binary);
            if (ofs) {
                for (const auto& t : localTRS) {
                    const float tvals[3] = { t.translation.x, t.translation.y, t.translation.z };
                    const float rvals[4] = { t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w }; // xyzw
                    const float svals[3] = { t.scale.x, t.scale.y, t.scale.z };
                    ofs.write(reinterpret_cast<const char*>(tvals), sizeof(tvals));
                    ofs.write(reinterpret_cast<const char*>(rvals), sizeof(rvals));
                    ofs.write(reinterpret_cast<const char*>(svals), sizeof(svals));
                }
                ofs.close();
                std::cout << "[Export] Saved: " << binPath << "\n";
            }
        }
        // 6.3) Bounds.bin (scene-local)
        WriteBoundsBinaryTo(localBounds, sceneDir);

        // 6.4) Scene JSON
        json sroot = json::object();
        sroot["gltf_source"] = sm.gltf_source;
        // keep materials for reference (no remap)
        json sceneMaterials = json::array();
        for (const auto& mtl : sm.materials) {
            json mj = json::object();
            if (!mtl.name.empty()) mj["name"] = mtl.name;
            sceneMaterials.push_back(std::move(mj));
        }
        sroot["materials"] = std::move(sceneMaterials);

        // pools counts
        sroot["matrices"] = static_cast<int64_t>(localMatrices.size());
        sroot["trs_count"] = static_cast<int64_t>(localTRS.size());
        sroot["bounds"] = static_cast<int64_t>(localBounds.size());

        // subMeshes (scene-local)
        json localSM = json::array();
        for (const auto& p : localSubMeshes) {
            json sj = json::object();
            sj["geometry"] = static_cast<int64_t>(p.geometry); // geometry remains global index
            if (p.material) sj["material"] = static_cast<int64_t>(*p.material); else sj["material"] = nullptr;
            localSM.push_back(std::move(sj));
        }
        sroot["subMeshes"] = std::move(localSM);

        // nodes (scene-local)
        json localNodeArr = json::array();
        for (const auto& n : localNodes) {
            json j = json::object();
            if (!n.name.empty()) j["name"] = n.name;
            json ch = json::array();
            for (auto c : n.children) ch.push_back(static_cast<int64_t>(c));
            j["children"] = std::move(ch);
            j["matrix"] = static_cast<int64_t>(n.matrixIndexPlusOne);
            j["trs"] = static_cast<int64_t>(n.trsIndexPlusOne);
            json sms = json::array();
            for (auto idx : n.subMeshes) sms.push_back(static_cast<int64_t>(idx));
            j["subMeshes"] = std::move(sms);
            j["bounds"] = (n.boundsIndex == pure::kInvalidBoundsIndex) ? json(nullptr) : json(static_cast<int64_t>(n.boundsIndex));
            localNodeArr.push_back(std::move(j));
        }
        sroot["mesh_nodes"] = std::move(localNodeArr);

        // scene object (single)
        sroot["scene"] = std::move(sceneJson);

        std::filesystem::path sjsonPath = sceneDir / "StaticMesh.json";
        std::ofstream sjsonOut(sjsonPath, std::ios::binary);
        if (!sjsonOut) {
            std::cerr << "[Export] Cannot open: " << sjsonPath << "\n";
            return false;
        }
        sjsonOut << sroot.dump(4);
        sjsonOut.close();
        std::cout << "[Export] Saved: " << sjsonPath << "\n";

        // 6.5) Scene.bin (single binary scene output per request)
        WriteSceneBinary(sceneDir, sc.name, sceneRootIndices, baseName, localNodes, localSubMeshes, localMatrices, localTRS);
    }

    return true;
}

} // namespace exporters
