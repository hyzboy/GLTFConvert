#include "Exporter.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using nlohmann::json;
using namespace puregltf;

namespace exporters {

static std::string stem_noext(const std::filesystem::path& p) {
    return p.stem().string();
}

bool ExportPureModel(const Model& model, const std::filesystem::path& outDir) {
    std::filesystem::path baseDir = outDir.empty() ? std::filesystem::path(model.source).parent_path() : outDir;
    std::error_code ec;
    std::filesystem::create_directories(baseDir, ec);

    const std::string baseName = stem_noext(model.source);
    std::filesystem::path targetDir = baseDir / (baseName + ".StaticMesh");
    std::filesystem::create_directories(targetDir, ec);

    json root = json::object();
    root["gltf_source"] = model.source;

    // scenes
    json scenes = json::array();
    for (const auto& sc : model.scenes) {
        json s = json::object();
        if (!sc.name.empty()) s["name"] = sc.name;
        json ns = json::array();
        for (auto n : sc.nodes) ns.push_back(static_cast<int64_t>(n));
        s["nodes"] = std::move(ns);
        if (!sc.worldAABB.empty()) {
            s["aabb"] = json::object({
                {"min", json::array({sc.worldAABB.min.x, sc.worldAABB.min.y, sc.worldAABB.min.z})},
                {"max", json::array({sc.worldAABB.max.x, sc.worldAABB.max.y, sc.worldAABB.max.z})}
            });
        } else {
            s["aabb"] = nullptr;
        }
        scenes.push_back(std::move(s));
    }
    root["scenes"] = std::move(scenes);

    // nodes
    json nodes = json::array();
    for (const auto& n : model.nodes) {
        json j = json::object();
        if (!n.name.empty()) j["name"] = n.name;
        if (n.mesh) j["mesh"] = static_cast<int64_t>(*n.mesh);
        json ch = json::array();
        for (auto c : n.children) ch.push_back(static_cast<int64_t>(c));
        j["children"] = std::move(ch);

        if (n.hasMatrix) {
            json m = json::array();
            for (int c = 0; c < 4; ++c)
                for (int r = 0; r < 4; ++r)
                    m.push_back(n.matrix[c][r]);
            j["matrix"] = std::move(m);
        } else {
            j["transform"] = json::object({
                {"translation", json::array({n.translation.x, n.translation.y, n.translation.z})},
                {"rotation", json::array({n.rotation.x, n.rotation.y, n.rotation.z, n.rotation.w})},
                {"scale", json::array({n.scale.x, n.scale.y, n.scale.z})}
            });
        }

        // world matrix
        json wm = json::array();
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                wm.push_back(n.worldMatrix[c][r]);
        j["world_matrix"] = std::move(wm);

        nodes.push_back(std::move(j));
    }
    root["nodes"] = std::move(nodes);

    // primitives + meshes
    json primitives = json::array();
    json meshes = json::array();

    for (const auto& m : model.meshes) {
        json mj = json::object();
        if (!m.name.empty()) mj["name"] = m.name;
        json primIdx = json::array();
        for (auto pi : m.primitives) primIdx.push_back(static_cast<int64_t>(pi));
        mj["primitives"] = std::move(primIdx);
        meshes.push_back(std::move(mj));
    }

    for (std::size_t i = 0; i < model.primitives.size(); ++i) {
        const auto& p = model.primitives[i];
        json pj = json::object();
        pj["mode"] = p.mode;
        json attrs = json::array();
        for (const auto& a : p.attributes) {
            // write attribute blob
            std::filesystem::path binName = std::to_string(i) + "." + a.name;
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
                {"id", static_cast<int64_t>(attrs.size())},
                {"name", a.name},
                {"count", static_cast<int64_t>(a.count)},
                {"componentType", a.componentType},
                {"type", a.type}
            });
            attrs.push_back(std::move(aj));
        }
        pj["attributes"] = std::move(attrs);

        if (!p.localAABB.empty()) {
            pj["aabb"] = json::object({
                {"min", json::array({p.localAABB.min.x, p.localAABB.min.y, p.localAABB.min.z})},
                {"max", json::array({p.localAABB.max.x, p.localAABB.max.y, p.localAABB.max.z})}
            });
        }

        if (p.indices) {
            std::filesystem::path binName = std::to_string(i) + ".indices";
            std::filesystem::path binPath = targetDir / binName;
            std::ofstream ofs(binPath, std::ios::binary);
            if (ofs) {
                ofs.write(reinterpret_cast<const char*>(p.indices->data()), static_cast<std::streamsize>(p.indices->size()));
                ofs.close();
                std::cout << "[Export] Saved: " << binPath << "\n";
                pj["indices_file"] = binName.string();
            } else {
                std::cerr << "[Export] Write failed: " << binPath << "\n";
            }
        }

        primitives.push_back(std::move(pj));
    }

    root["meshes"] = std::move(meshes);
    root["primitives"] = std::move(primitives);

    std::filesystem::path jsonPath = targetDir / (baseName + ".json");
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
