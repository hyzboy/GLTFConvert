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
#include "SceneLocal.h"

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

// --- New helper functions splitting ExportPureModel by top-level JSON nodes ---

static json BuildMaterialsJson(const pure::Model& sm)
{
    json materials = json::array();
    for (const auto& mtl : sm.materials) {
        json mj = json::object();
        if (!mtl.name.empty()) mj["name"] = mtl.name;
        materials.push_back(std::move(mj));
    }
    return materials;
}

static json BuildScenesJson(const pure::Model& sm)
{
    json scenes = json::array();
    for (const auto& sc : sm.scenes) {
        json s = json::object();
        if (!sc.name.empty()) s["name"] = sc.name;
        json ns = json::array();
        for (auto n : sc.nodes) ns.push_back(static_cast<int64_t>(n));
        s["nodes"] = std::move(ns);
        s["bounds"] = (sc.boundsIndex == pure::kInvalidBoundsIndex) ? json(nullptr) : json(static_cast<int64_t>(sc.boundsIndex));
        scenes.push_back(std::move(s));
    }
    return scenes;
}

static json BuildMeshNodesJson(const pure::Model& sm)
{
    json meshNodes = json::array();
    for (const auto& n : sm.mesh_nodes) {
        json j = json::object();
        if (!n.name.empty()) j["name"] = n.name;
        json ch = json::array();
        for (auto c : n.children) ch.push_back(static_cast<int64_t>(c));
        j["children"] = std::move(ch);
        j["matrix"] = static_cast<int64_t>(n.matrixIndexPlusOne);
        j["trs"] = static_cast<int64_t>(n.trsIndexPlusOne);
        json sms = json::array();
        for (auto primIndex : n.subMeshes) sms.push_back(static_cast<int64_t>(primIndex));
        j["subMeshes"] = std::move(sms);
        j["bounds"] = (n.boundsIndex == pure::kInvalidBoundsIndex) ? json(nullptr) : json(static_cast<int64_t>(n.boundsIndex));
        meshNodes.push_back(std::move(j));
    }
    return meshNodes;
}

static json BuildSubMeshesJson(const pure::Model& sm)
{
    json subMeshes = json::array();
    for (const auto& p : sm.subMeshes) {
        json sj = json::object();
        sj["geometry"] = static_cast<int64_t>(p.geometry);
        if (p.material) sj["material"] = static_cast<int64_t>(*p.material); else sj["material"] = nullptr;
        subMeshes.push_back(std::move(sj));
    }
    return subMeshes;
}

static void WriteGlobalMatrices(json& root, const pure::Model& sm, const std::filesystem::path& targetDir)
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
    root["matrices"] = static_cast<int64_t>(sm.matrixPool.size());
}

static void WriteGlobalTRS(json& root, const pure::Model& sm, const std::filesystem::path& targetDir)
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
    root["trs_count"] = static_cast<int64_t>(sm.trsPool.size());
}

static void ExportGeometries(const pure::Model& sm, const std::filesystem::path& targetDir)
{
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
}

static bool ExportScene(const pure::Model& sm, std::size_t si, const std::filesystem::path& targetDir, const std::string& baseName)
{
    const auto& sc = sm.scenes[si];
    pure::SceneLocal sl = pure::BuildSceneLocal(sm, si);

    // scene folder
    std::string sceneFolderName;
    {
        std::string sane = SanitizeName(sc.name);
        if (sane.empty()) sceneFolderName = "Scene_" + std::to_string(si);
        else sceneFolderName = std::to_string(si) + "_" + sane;
    }
    std::filesystem::path sceneDir = targetDir / sceneFolderName;
    std::error_code ec;
    std::filesystem::create_directories(sceneDir, ec);

    // Matrices.bin
    {
        std::filesystem::path binPath = sceneDir / "Matrices.bin";
        std::ofstream ofs(binPath, std::ios::binary);
        if (ofs) {
            for (const auto& m : sl.matrixPool) {
                ofs.write(reinterpret_cast<const char*>(&m.local), sizeof(glm::mat4));
                ofs.write(reinterpret_cast<const char*>(&m.world), sizeof(glm::mat4));
            }
            ofs.close();
            std::cout << "[Export] Saved: " << binPath << "\n";
        }
    }
    // TRS.bin
    {
        std::filesystem::path binPath = sceneDir / "TRS.bin";
        std::ofstream ofs(binPath, std::ios::binary);
        if (ofs) {
            for (const auto& t : sl.trsPool) {
                const float tvals[3] = { t.translation.x, t.translation.y, t.translation.z };
                const float rvals[4] = { t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w };
                const float svals[3] = { t.scale.x, t.scale.y, t.scale.z };
                ofs.write(reinterpret_cast<const char*>(tvals), sizeof(tvals));
                ofs.write(reinterpret_cast<const char*>(rvals), sizeof(rvals));
                ofs.write(reinterpret_cast<const char*>(svals), sizeof(svals));
            }
            ofs.close();
            std::cout << "[Export] Saved: " << binPath << "\n";
        }
    }
    // Bounds.bin
    WriteBoundsBinaryTo(sl.bounds, sceneDir);

    // Scene JSON
    json sroot = json::object();
    sroot["gltf_source"] = sm.gltf_source;
    // keep materials for reference (no remap)
    sroot["materials"] = BuildMaterialsJson(sm);

    // pools counts
    sroot["matrices"] = static_cast<int64_t>(sl.matrixPool.size());
    sroot["trs_count"] = static_cast<int64_t>(sl.trsPool.size());
    sroot["bounds"] = static_cast<int64_t>(sl.bounds.size());

    // subMeshes (scene-local)
    json localSM = json::array();
    for (const auto& p : sl.subMeshes) {
        json sj = json::object();
        sj["geometry"] = static_cast<int64_t>(p.geometry);
        if (p.material) sj["material"] = static_cast<int64_t>(*p.material); else sj["material"] = nullptr;
        localSM.push_back(std::move(sj));
    }
    sroot["subMeshes"] = std::move(localSM);

    // nodes (scene-local)
    json localNodeArr = json::array();
    for (const auto& n : sl.nodes) {
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
    json sceneJson = json::object();
    if (!sc.name.empty()) sceneJson["name"] = sc.name;
    json roots = json::array();
    for (auto r : sl.roots) roots.push_back(static_cast<int64_t>(r));
    sceneJson["nodes"] = std::move(roots);
    if (sl.sceneBoundsIndex != pure::kInvalidBoundsIndex) sceneJson["bounds"] = static_cast<int64_t>(sl.sceneBoundsIndex); else sceneJson["bounds"] = nullptr;
    sroot["scene"] = std::move(sceneJson);

    std::filesystem::path sjsonPath = sceneDir / (sc.name+".json");
    std::ofstream sjsonOut(sjsonPath, std::ios::binary);
    if (!sjsonOut) {
        std::cerr << "[Export] Cannot open: " << sjsonPath << "\n";
        return false;
    }
    sjsonOut << sroot.dump(4);
    sjsonOut.close();
    std::cout << "[Export] Saved: " << sjsonPath << "\n";

    // Scene.bin (binary, no materials/bounds; geometry file names only)
    WriteSceneBinary(sceneDir, sc.name, sl.roots, baseName, sl.nodes, sl.subMeshes, sl.matrixPool, sl.trsPool);

    return true;
}

// --- End helpers ---

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
    root["materials"] = BuildMaterialsJson(sm);

    // Write bounds binary once and only store count in JSON (global)
    if (!WriteAllBoundsBinary(sm, targetDir)) {
        std::cerr << "[Export] Failed to write bounds binary." << "\n";
    }
    root["bounds"] = static_cast<int64_t>(sm.bounds.size());

    // scenes (global listing with original indices, preserved for legacy)
    root["scenes"] = BuildScenesJson(sm);

    // mesh_nodes and transform indices (subMeshes are already attached in pure model)
    root["mesh_nodes"] = BuildMeshNodesJson(sm);

    // Export matrix pool as a binary file (local/world pairs) and store count
    WriteGlobalMatrices(root, sm, targetDir);

    // Export TRS pool as a compact binary file and store count
    WriteGlobalTRS(root, sm, targetDir);

    // Only export consolidated .geometry files; drop per-attribute/indices files and geometry JSON
    ExportGeometries(sm, targetDir);

    // Global SubMeshes list (kept; references geometry indices that map to <index>.geometry files)
    root["subMeshes"] = BuildSubMeshesJson(sm);

    std::filesystem::path jsonPath = targetDir / ("StaticMesh.json");
    std::ofstream jsonOut(jsonPath, std::ios::binary);
    if (!jsonOut) {
        std::cerr << "[Export] Cannot open: " << jsonPath << "\n";
        return false;
    }

    jsonOut << root.dump(4);
    jsonOut.close();
    std::cout << "[Export] Saved: " << jsonPath << "\n";

    // Per-scene independent exports using SceneLocal to simplify logic
    for (std::size_t si = 0; si < sm.scenes.size(); ++si) {
        if (!ExportScene(sm, si, targetDir, baseName)) return false;
    }

    return true;
}

} // namespace exporters
