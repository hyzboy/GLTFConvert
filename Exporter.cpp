#include "Exporter.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <algorithm>

using nlohmann::json;
using namespace puregltf;

namespace exporters {

static std::string stem_noext(const std::filesystem::path& p) {
    return p.stem().string();
}

// Compare two primitives' geometry by accessor identity (mode, attributes set, indices accessor)
static bool SameGeometryByAccessorId(const Primitive& a, const Primitive& b) {
    if (a.mode != b.mode) return false;

    // indices presence and identity
    if (static_cast<bool>(a.indicesAccessorIndex) != static_cast<bool>(b.indicesAccessorIndex)) return false;
    if (a.indicesAccessorIndex && b.indicesAccessorIndex && (*a.indicesAccessorIndex != *b.indicesAccessorIndex)) return false;

    if (a.attributes.size() != b.attributes.size()) return false;

    // Build sorted views of (name, accessorIndex)
    auto makeList = [](const Primitive& p) {
        std::vector<std::pair<std::string, std::size_t>> v;
        v.reserve(p.attributes.size());
        for (const auto& at : p.attributes) {
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

bool ExportPureModel(const Model& model, const std::filesystem::path& outDir) {
    std::filesystem::path baseDir = outDir.empty() ? std::filesystem::path(model.source).parent_path() : outDir;
    std::error_code ec;
    std::filesystem::create_directories(baseDir, ec);

    const std::string baseName = stem_noext(model.source);
    std::filesystem::path targetDir = baseDir / (baseName + ".StaticMesh");
    std::filesystem::create_directories(targetDir, ec);

    json root = json::object();
    root["gltf_source"] = model.source;

    // materials (names only for now)
    json materials = json::array();
    for (const auto& mtl : model.materials) {
        json mj = json::object();
        if (!mtl.name.empty()) mj["name"] = mtl.name;
        materials.push_back(std::move(mj));
    }
    root["materials"] = std::move(materials);

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

    // mesh_nodes (rename of nodes) and world transforms
    json meshNodes = json::array();
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

        meshNodes.push_back(std::move(j));
    }
    root["mesh_nodes"] = std::move(meshNodes);

    // Build unique geometry set by accessor identity
    const auto& prims = model.primitives;
    std::vector<std::size_t> uniqueRepPrimIdx; // representative primitive index per unique geometry
    std::vector<std::size_t> geomIndexOfPrim(prims.size(), static_cast<std::size_t>(-1));

    for (std::size_t i = 0; i < prims.size(); ++i) {
        const auto& p = prims[i];
        std::size_t found = static_cast<std::size_t>(-1);
        for (std::size_t u = 0; u < uniqueRepPrimIdx.size(); ++u) {
            if (SameGeometryByAccessorId(p, prims[uniqueRepPrimIdx[u]])) { found = u; break; }
        }
        if (found == static_cast<std::size_t>(-1)) {
            uniqueRepPrimIdx.push_back(i);
            geomIndexOfPrim[i] = uniqueRepPrimIdx.size() - 1;
        } else {
            geomIndexOfPrim[i] = found;
        }
    }

    // geometry (pure geometry) and binary dumps for unique ones only
    json geometry = json::array();

    for (std::size_t u = 0; u < uniqueRepPrimIdx.size(); ++u) {
        const std::size_t i = uniqueRepPrimIdx[u];
        const auto& p = prims[i];
        json pj = json::object();
        pj["mode"] = p.mode;
        json attrs = json::array();
        for (const auto& a : p.attributes) {
            // write attribute blob (geometry only) named by unique geometry index
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
            // still write index buffer for consumers, but do not store file name in JSON
            std::filesystem::path binName = std::to_string(u) + ".indices";
            std::filesystem::path binPath = targetDir / binName;
            std::ofstream ofs(binPath, std::ios::binary);
            if (ofs) {
                ofs.write(reinterpret_cast<const char*>(p.indices->data()), static_cast<std::streamsize>(p.indices->size()));
                ofs.close();
                std::cout << "[Export] Saved: " << binPath << "\n";
            } else {
                std::cerr << "[Export] Write failed: " << binPath << "\n";
            }
        }

        // export index metadata instead of file name
        if (p.indexCount && p.indexComponentType) {
            pj["indices"] = json::object({
                {"count", static_cast<int64_t>(*p.indexCount)},
                {"componentType", *p.indexComponentType}
            });
        } else {
            pj["indices"] = nullptr;
        }

        geometry.push_back(std::move(pj));
    }

    root["geometry"] = std::move(geometry);

    // SubMeshes: one-to-one with glTF primitives: geometry + material
    json subMeshes = json::array();
    for (std::size_t i = 0; i < prims.size(); ++i) {
        const auto& p = prims[i];
        json sj = json::object();
        sj["geometry"] = static_cast<int64_t>(geomIndexOfPrim[i]);
        if (p.material) sj["material"] = static_cast<int64_t>(*p.material); else sj["material"] = nullptr;
        subMeshes.push_back(std::move(sj));
    }
    root["subMeshes"] = std::move(subMeshes);

    // Attach per-node subMeshes list (derived from glTF node.mesh -> mesh.primitives)
    {
        auto& meshNodesRef = root["mesh_nodes"];
        for (std::size_t ni = 0; ni < model.nodes.size(); ++ni) {
            const auto& node = model.nodes[ni];
            json sms = json::array();
            if (node.mesh) {
                const auto& mesh = model.meshes[*node.mesh];
                for (auto primIndex : mesh.primitives) {
                    // push subMesh index (one-to-one with glTF primitives)
                    sms.push_back(static_cast<int64_t>(primIndex));
                }
            }
            meshNodesRef[static_cast<int>(ni)]["subMeshes"] = std::move(sms);
        }
    }

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
