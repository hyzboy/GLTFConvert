#include "Exporter.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <algorithm>

#include "StaticMesh.h"

using nlohmann::json;

namespace exporters {

static std::string stem_noext(const std::filesystem::path& p) {
    return p.stem().string();
}

static void writeOBB(json& j, const ::OBB& obb)
{
    j = json::object({
        {"center", json::array({obb.center.x, obb.center.y, obb.center.z})},
        {"axisX", json::array({obb.axisX.x, obb.axisX.y, obb.axisX.z})},
        {"axisY", json::array({obb.axisY.x, obb.axisY.y, obb.axisY.z})},
        {"axisZ", json::array({obb.axisZ.x, obb.axisZ.y, obb.axisZ.z})},
        {"halfSize", json::array({obb.halfSize.x, obb.halfSize.y, obb.halfSize.z})}
    });
}

static void writeBounds(json& j, const BoundingBox& b)
{
    json out = json::object();
    if (!b.aabb.empty()) {
        out["aabb"] = json::object({
            {"min", json::array({b.aabb.min.x, b.aabb.min.y, b.aabb.min.z})},
            {"max", json::array({b.aabb.max.x, b.aabb.max.y, b.aabb.max.z})}
        });
    } else {
        out["aabb"] = nullptr;
    }
    if (!b.obb.empty()) {
        json jobb; writeOBB(jobb, b.obb);
        out["obb"] = std::move(jobb);
    } else {
        out["obb"] = nullptr;
    }
    if (!b.sphere.empty()) {
        out["sphere"] = json::object({
            {"center", json::array({b.sphere.center.x, b.sphere.center.y, b.sphere.center.z})},
            {"radius", b.sphere.radius}
        });
    } else {
        out["sphere"] = nullptr;
    }
    j = std::move(out);
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

    // scenes
    json scenes = json::array();
    for (const auto& sc : sm.scenes) {
        json s = json::object();
        if (!sc.name.empty()) s["name"] = sc.name;
        json ns = json::array();
        for (auto n : sc.nodes) ns.push_back(static_cast<int64_t>(n));
        s["nodes"] = std::move(ns);

        json jb; writeBounds(jb, sc.bounds);
        s["bounds"] = std::move(jb);

        scenes.push_back(std::move(s));
    }
    root["scenes"] = std::move(scenes);

    // mesh_nodes and world transforms (subMeshes are already attached in pure model)
    json meshNodes = json::array();
    for (const auto& n : sm.mesh_nodes) {
        json j = json::object();
        if (!n.name.empty()) j["name"] = n.name;
        json ch = json::array();
        for (auto c : n.children) ch.push_back(static_cast<int64_t>(c));
        j["children"] = std::move(ch);

        if (n.matrix.has_value()) {
            json m = json::array();
            for (int c = 0; c < 4; ++c)
                for (int r = 0; r < 4; ++r)
                    m.push_back(n.matrix->operator[](c)[r]);
            j["matrix"] = std::move(m);
        } else if (n.transform.has_value()) {
            const auto& tf = *n.transform;
            j["transform"] = json::object({
                {"translation", json::array({tf.translation.x, tf.translation.y, tf.translation.z})},
                {"rotation", json::array({tf.rotation.x, tf.rotation.y, tf.rotation.z, tf.rotation.w})},
                {"scale", json::array({tf.scale.x, tf.scale.y, tf.scale.z})}
            });
        } else {
            j["transform"] = nullptr;
        }

        // world matrix
        json wm = json::array();
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                wm.push_back(n.world_matrix[c][r]);
        j["world_matrix"] = std::move(wm);

        // per-node subMeshes (indices into global subMeshes)
        json sms = json::array();
        for (auto primIndex : n.subMeshes) sms.push_back(static_cast<int64_t>(primIndex));
        j["subMeshes"] = std::move(sms);

        json jb; writeBounds(jb, n.bounds);
        j["bounds"] = std::move(jb);

        meshNodes.push_back(std::move(j));
    }
    root["mesh_nodes"] = std::move(meshNodes);

    // geometry (pure geometry) and binary dumps
    json geometry = json::array();

    for (std::size_t u = 0; u < sm.geometry.size(); ++u) {
        const auto& g = sm.geometry[u];
        json pj = json::object();
        pj["mode"] = g.mode;
        json attrs = json::array();
        for (const auto& a : g.attributes) {
            // write attribute blob named by unique geometry index
            std::filesystem::path binName = std::to_string(u) + "." + a.name;
            std::filesystem::path binPath = targetDir / binName;
            std::ofstream ofs(binPath, std::ios::binary);
            if (ofs) {
                ofs.write(reinterpret_cast<const char*>(a.data.data()), static_cast<std::streamsize>(a.data.size()));
                ofs.close();
                std::cout << "[Export] Saved: " << binPath << "\n";
            } else {
                std::cerr << "[Export] Write failed: " << binPath << "\n";
            }

            json aj = json::object({
                {"id", static_cast<int64_t>(a.id)},
                {"name", a.name},
                {"count", static_cast<int64_t>(a.count)},
                {"componentType", a.componentType},
                {"type", a.type}
            });
            attrs.push_back(std::move(aj));
        }
        pj["attributes"] = std::move(attrs);

        json jb; writeBounds(jb, g.bounds);
        pj["bounds"] = std::move(jb);

        if (g.indicesData.has_value()) {
            // write index buffer for consumers, but do not store file name in JSON
            std::filesystem::path binName = std::to_string(u) + ".indices";
            std::filesystem::path binPath = targetDir / binName;
            std::ofstream ofs(binPath, std::ios::binary);
            if (ofs) {
                const auto& ib = *g.indicesData;
                ofs.write(reinterpret_cast<const char*>(ib.data()), static_cast<std::streamsize>(ib.size()));
                ofs.close();
                std::cout << "[Export] Saved: " << binPath << "\n";
            } else {
                std::cerr << "[Export] Write failed: " << binPath << "\n";
            }
        }

        // export index metadata instead of file name
        if (g.indices.has_value()) {
            pj["indices"] = json::object({
                {"count", static_cast<int64_t>(g.indices->count)},
                {"componentType", g.indices->componentType}
            });
        } else {
            pj["indices"] = nullptr;
        }

        geometry.push_back(std::move(pj));
    }

    root["geometry"] = std::move(geometry);

    // Global SubMeshes list
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
