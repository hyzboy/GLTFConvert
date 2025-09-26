#include "Exporter.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cstdio>

#include "StaticMesh.h"
#include "VKFormat.h"
#include "VertexAttrib.h"

using nlohmann::json;

namespace exporters {

static std::string stem_noext(const std::filesystem::path& p) {
    return p.stem().string();
}

static const char* VkFormatToString(VkFormat f) {
    switch (f) {
        case VK_FORMAT_UNDEFINED: return "VK_FORMAT_UNDEFINED";
        case VK_FORMAT_R8_UNORM: return "VK_FORMAT_R8_UNORM";
        case VK_FORMAT_R8_SNORM: return "VK_FORMAT_R8_SNORM";
        case VK_FORMAT_R8_UINT: return "VK_FORMAT_R8_UINT";
        case VK_FORMAT_R8_SINT: return "VK_FORMAT_R8_SINT";
        case VK_FORMAT_R16_UNORM: return "VK_FORMAT_R16_UNORM";
        case VK_FORMAT_R16_SINT: return "VK_FORMAT_R16_SINT";
        case VK_FORMAT_R16_UINT: return "VK_FORMAT_R16_UINT";
        case VK_FORMAT_R16_SFLOAT: return "VK_FORMAT_R16_SFLOAT";
        case VK_FORMAT_R32_UINT: return "VK_FORMAT_R32_UINT";
        case VK_FORMAT_R32_SINT: return "VK_FORMAT_R32_SINT";
        case VK_FORMAT_R32_SFLOAT: return "VK_FORMAT_R32_SFLOAT";
        case VK_FORMAT_R32G32_SFLOAT: return "VK_FORMAT_R32G32_SFLOAT";
        case VK_FORMAT_R32G32B32_SFLOAT: return "VK_FORMAT_R32G32B32_SFLOAT";
        case VK_FORMAT_R32G32B32A32_SFLOAT: return "VK_FORMAT_R32G32B32A32_SFLOAT";
        case VK_FORMAT_R64_SFLOAT: return "VK_FORMAT_R64_SFLOAT";
        case VK_FORMAT_R64G64_SFLOAT: return "VK_FORMAT_R64G64_SFLOAT";
        case VK_FORMAT_R64G64B64_SFLOAT: return "VK_FORMAT_R64G64B64_SFLOAT";
        case VK_FORMAT_R64G64B64A64_SFLOAT: return "VK_FORMAT_R64G64B64A64_SFLOAT";
        case VK_FORMAT_R8G8_UNORM: return "VK_FORMAT_R8G8_UNORM";
        case VK_FORMAT_R8G8B8A8_UNORM: return "VK_FORMAT_R8G8B8A8_UNORM";
        case VK_FORMAT_B8G8R8A8_UNORM: return "VK_FORMAT_B8G8R8A8_UNORM";
        case VK_FORMAT_R16G16_SFLOAT: return "VK_FORMAT_R16G16_SFLOAT";
        case VK_FORMAT_R16G16B16A16_SFLOAT: return "VK_FORMAT_R16G16B16A16_SFLOAT";
        // add more cases as needed
        default: {
            static thread_local char buf[64];
            std::snprintf(buf, sizeof(buf), "VkFormat(%d)", static_cast<int>(f));
            return buf;
        }
    }
}

static std::string IndexTypeToString(IndexType it) {
    switch (it) {
        case AUTO: return "AUTO";
        case U16: return "U16";
        case U32: return "U32";
        case U8: return "U8";
        case ERR: return "ERR";
        default: return std::to_string(static_cast<int>(it));
    }
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

    // geometry (pure geometry) and binary dumps
    json geometry = json::array();

    for (std::size_t u = 0; u < sm.geometry.size(); ++u) {
        const auto& g = sm.geometry[u];

        {
            std::filesystem::path binName = std::to_string(u) + ".geometry";
            std::filesystem::path binPath = targetDir / binName;

            if (!pure::SaveGeometry(g, binPath.string())) {
                std::cerr << "[Export] Failed to write geometry binary: " << binPath << "\n";
            }
        }

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
                {"type", a.type},
                {"format", VkFormatToString(a.format)}
            });
            attrs.push_back(std::move(aj));
        }
        pj["attributes"] = std::move(attrs);

        // reference bounds by index
        pj["bounds"] = (g.boundsIndex == pure::kInvalidBoundsIndex) ? json(nullptr) : json(static_cast<int64_t>(g.boundsIndex));

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
                {"componentType", g.indices->componentType},
                {"indexType", IndexTypeToString(g.indices->indexType)}
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
