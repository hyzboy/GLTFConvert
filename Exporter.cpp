#include "Exporter.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

#include "StaticMesh.h"

using nlohmann::json;

namespace exporters {

static std::string stem_noext(const std::filesystem::path& p) {
    return p.stem().string();
}

// Helper to write all bounds as a single binary file (Bounds.bin)
static bool WriteAllBoundsBinary(const pure::Model& sm, const std::filesystem::path& dir)
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
    for (const auto& b : sm.bounds) {
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

    // Write bounds binary once and only store count in JSON
    if (!WriteAllBoundsBinary(sm, targetDir)) {
        std::cerr << "[Export] Failed to write bounds binary." << "\n";
    }
    root["bounds"] = static_cast<int64_t>(sm.bounds.size());

    // scenes
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
    return true;
}

} // namespace exporters
