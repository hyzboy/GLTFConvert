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

namespace exporters
{

// ------------------------------------------------------------
// Utility helpers
// ------------------------------------------------------------
static std::string stem_noext(const std::filesystem::path &p)
{
    return p.stem().string();
}

static bool WriteBoundsBinaryTo(
    const std::vector<BoundingBox>   &bounds,
    const std::filesystem::path      &dir
)
{
    std::filesystem::path binPath = dir / "Bounds.bin";
    std::ofstream ofs(binPath, std::ios::binary);
    if (!ofs)
    {
        std::cerr << "[Export] Cannot open bounds binary: " << binPath << "\n";
        return false;
    }

    auto writeVec3 = [&](const glm::dvec3 &v)
    {
        double d[3] = { v.x, v.y, v.z };
        ofs.write(reinterpret_cast<const char *>(d), sizeof(d));
    };
    auto writeDouble = [&](double d)
    {
        ofs.write(reinterpret_cast<const char *>(&d), sizeof(d));
    };

    for (const auto &b : bounds)
    {
        writeVec3(b.aabb.min);
        writeVec3(b.aabb.max);
        writeVec3(b.obb.center);
        writeVec3(b.obb.axisX);
        writeVec3(b.obb.axisY);
        writeVec3(b.obb.axisZ);
        writeVec3(b.obb.halfSize);
        writeVec3(b.sphere.center);
        writeDouble(b.sphere.radius);
    }

    ofs.close();
    std::cout << "[Export] Saved: " << binPath << "\n";
    return true;
}

static bool WriteAllBoundsBinary(
    const pure::Model            &sm,
    const std::filesystem::path  &dir
)
{
    return WriteBoundsBinaryTo(sm.bounds, dir);
}

static std::string SanitizeName(const std::string &in)
{
    if (in.empty())
        return {};

    std::string out;
    out.reserve(in.size());
    for (char c : in)
    {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-')
            out.push_back(c);
        else
            out.push_back('_');
    }
    return out;
}

// ------------------------------------------------------------
// JSON builders (global model)
// ------------------------------------------------------------
static json BuildMaterialsJson(const pure::Model &sm)
{
    json arr = json::array();
    for (const auto &m : sm.materials)
    {
        json mj = json::object();
        if (!m.name.empty())
            mj["name"] = m.name;
        arr.push_back(std::move(mj));
    }
    return arr;
}

static json BuildScenesJson(const pure::Model &sm)
{
    json arr = json::array();
    for (const auto &sc : sm.scenes)
    {
        json s = json::object();
        if (!sc.name.empty())
            s["name"] = sc.name;
        json ns = json::array();
        for (auto n : sc.nodes)
            ns.push_back(static_cast<int64_t>(n));
        s["nodes"] = std::move(ns);
        s["bounds"] = (sc.boundsIndex == pure::kInvalidBoundsIndex)
            ? json(nullptr)
            : json(static_cast<int64_t>(sc.boundsIndex));
        arr.push_back(std::move(s));
    }
    return arr;
}

static json BuildMeshNodesJson(const pure::Model &sm)
{
    json arr = json::array();
    for (const auto &n : sm.mesh_nodes)
    {
        json j = json::object();
        if (!n.name.empty())
            j["name"] = n.name;

        json ch = json::array();
        for (auto c : n.children)
            ch.push_back(static_cast<int64_t>(c));
        j["children"] = std::move(ch);

        j["localMatrix"] = static_cast<int64_t>(n.localMatrixIndexPlusOne);
        j["worldMatrix"] = static_cast<int64_t>(n.worldMatrixIndexPlusOne);
        j["trs"]         = static_cast<int64_t>(n.trsIndexPlusOne);

        json sms = json::array();
        for (auto smi : n.subMeshes)
            sms.push_back(static_cast<int64_t>(smi));
        j["subMeshes"] = std::move(sms);

        j["bounds"] = (n.boundsIndex == pure::kInvalidBoundsIndex)
            ? json(nullptr)
            : json(static_cast<int64_t>(n.boundsIndex));

        arr.push_back(std::move(j));
    }
    return arr;
}

static json BuildSubMeshesJson(const pure::Model &sm)
{
    json arr = json::array();
    for (const auto &p : sm.subMeshes)
    {
        json sj        = json::object();
        sj["geometry"] = static_cast<int64_t>(p.geometry);
        if (p.material)
            sj["material"] = static_cast<int64_t>(*p.material);
        else
            sj["material"] = nullptr;
        arr.push_back(std::move(sj));
    }
    return arr;
}

// ------------------------------------------------------------
// Binary dumps (global)
// ------------------------------------------------------------
static void WriteGlobalMatrices(
    json                       &root,
    const pure::Model          &sm,
    const std::filesystem::path &targetDir
)
{
    std::filesystem::path dataPath = targetDir / "MatrixData.bin";
    {
        std::ofstream ofs(dataPath, std::ios::binary);
        if (ofs)
        {
            if (!sm.matrixData.empty())
                ofs.write(reinterpret_cast<const char *>(sm.matrixData.data()), sm.matrixData.size() * sizeof(glm::mat4));
            ofs.close();
            std::cout << "[Export] Saved: " << dataPath << "\n";
        }
    }
    root["matrix_data"] = static_cast<int64_t>(sm.matrixData.size());
}

static void WriteGlobalTRS(
    json                       &root,
    const pure::Model          &sm,
    const std::filesystem::path &targetDir
)
{
    std::filesystem::path binPath = targetDir / "TRS.bin";
    std::ofstream ofs(binPath, std::ios::binary);
    if (ofs)
    {
        for (const auto &t : sm.trsPool)
        {
            const float tvals[3] = { t.translation.x, t.translation.y, t.translation.z };
            const float rvals[4] = { t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w };
            const float svals[3] = { t.scale.x, t.scale.y, t.scale.z };
            ofs.write(reinterpret_cast<const char *>(tvals), sizeof(tvals));
            ofs.write(reinterpret_cast<const char *>(rvals), sizeof(rvals));
            ofs.write(reinterpret_cast<const char *>(svals), sizeof(svals));
        }
        ofs.close();
        std::cout << "[Export] Saved: " << binPath << "\n";
    }
    root["trs_count"] = static_cast<int64_t>(sm.trsPool.size());
}

static void ExportGeometries(
    const pure::Model           &sm,
    const std::filesystem::path &targetDir
)
{
    for (std::size_t u = 0; u < sm.geometry.size(); ++u)
    {
        const auto &g = sm.geometry[u];
        std::filesystem::path binName = stem_noext(sm.gltf_source) + "." + std::to_string(u) + ".geometry";
        std::filesystem::path binPath = targetDir / binName;
        const BoundingBox &geo_bounds = (g.boundsIndex != pure::kInvalidBoundsIndex)
            ? sm.bounds[g.boundsIndex]
            : BoundingBox{};
        if (!pure::SaveGeometry(g, geo_bounds, binPath.string()))
            std::cerr << "[Export] Failed to write geometry binary: " << binPath << "\n";
    }
}

// ------------------------------------------------------------
// Per-scene export
// ------------------------------------------------------------
static bool ExportScene(
    const pure::Model           &sm,
    std::size_t                  si,
    const std::filesystem::path &targetDir,
    const std::string           &baseName
)
{
    const auto &sc = sm.scenes[si];
    pure::SceneLocal sl = pure::BuildSceneLocal(sm, static_cast<int32_t>(si));

    std::string sceneFolderName;
    {
        std::string sane = SanitizeName(sc.name);
        if (sane.empty())
            sceneFolderName = "Scene_" + std::to_string(si);
        else
            sceneFolderName = std::to_string(si) + "_" + sane;
    }

    std::filesystem::path sceneDir = targetDir / sceneFolderName;
    std::error_code ec;
    std::filesystem::create_directories(sceneDir, ec);

    // MatrixData.bin
    {
        std::filesystem::path binPath = sceneDir / "MatrixData.bin";
        std::ofstream ofs(binPath, std::ios::binary);
        if (ofs)
        {
            if (!sl.matrixData.empty())
                ofs.write(reinterpret_cast<const char *>(sl.matrixData.data()), sl.matrixData.size() * sizeof(glm::mat4));
            ofs.close();
            std::cout << "[Export] Saved: " << binPath << "\n";
        }
    }

    // TRS.bin
    {
        std::filesystem::path binPath = sceneDir / "TRS.bin";
        std::ofstream ofs(binPath, std::ios::binary);
        if (ofs)
        {
            for (const auto &t : sl.trsPool)
            {
                const float tvals[3] = { t.translation.x, t.translation.y, t.translation.z };
                const float rvals[4] = { t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w };
                const float svals[3] = { t.scale.x, t.scale.y, t.scale.z };
                ofs.write(reinterpret_cast<const char *>(tvals), sizeof(tvals));
                ofs.write(reinterpret_cast<const char *>(rvals), sizeof(rvals));
                ofs.write(reinterpret_cast<const char *>(svals), sizeof(svals));
            }
            ofs.close();
            std::cout << "[Export] Saved: " << binPath << "\n";
        }
    }

    WriteBoundsBinaryTo(sl.bounds, sceneDir);

    // Build scene-local JSON structure
    json sroot = json::object();
    sroot["gltf_source"] = sm.gltf_source;
    sroot["materials"]   = BuildMaterialsJson(sm);
    sroot["matrix_data"] = static_cast<int64_t>(sl.matrixData.size());
    sroot["trs_count"]   = static_cast<int64_t>(sl.trsPool.size());
    sroot["bounds"]      = static_cast<int64_t>(sl.bounds.size());

    json localSM = json::array();
    for (const auto &p : sl.subMeshes)
    {
        json sj        = json::object();
        sj["geometry"] = static_cast<int64_t>(p.geometry);
        if (p.material)
            sj["material"] = static_cast<int64_t>(*p.material);
        else
            sj["material"] = nullptr;
        localSM.push_back(std::move(sj));
    }
    sroot["subMeshes"] = std::move(localSM);

    json localNodeArr = json::array();
    for (const auto &n : sl.nodes)
    {
        json j = json::object();
        if (!n.name.empty())
            j["name"] = n.name;

        json ch = json::array();
        for (auto c : n.children)
            ch.push_back(static_cast<int64_t>(c));
        j["children"]   = std::move(ch);
        j["localMatrix"] = static_cast<int64_t>(n.localMatrixIndexPlusOne);
        j["worldMatrix"] = static_cast<int64_t>(n.worldMatrixIndexPlusOne);
        j["trs"]         = static_cast<int64_t>(n.trsIndexPlusOne);

        json sms = json::array();
        for (auto idx : n.subMeshes)
            sms.push_back(static_cast<int64_t>(idx));
        j["subMeshes"] = std::move(sms);

        j["bounds"] = (n.boundsIndex == pure::kInvalidBoundsIndex)
            ? json(nullptr)
            : json(static_cast<int64_t>(n.boundsIndex));

        localNodeArr.push_back(std::move(j));
    }
    sroot["mesh_nodes"] = std::move(localNodeArr);

    json sceneJson = json::object();
    if (!sc.name.empty())
        sceneJson["name"] = sc.name;

    json roots = json::array();
    for (auto r : sl.roots)
        roots.push_back(static_cast<int64_t>(r));
    sceneJson["nodes"] = std::move(roots);

    if (sl.sceneBoundsIndex != pure::kInvalidBoundsIndex)
        sceneJson["bounds"] = static_cast<int64_t>(sl.sceneBoundsIndex);
    else
        sceneJson["bounds"] = nullptr;

    sroot["scene"] = std::move(sceneJson);

    std::filesystem::path sjsonPath = sceneDir / (sc.name + ".json");
    std::ofstream sjsonOut(sjsonPath, std::ios::binary);
    if (!sjsonOut)
    {
        std::cerr << "[Export] Cannot open: " << sjsonPath << "\n";
        return false;
    }
    sjsonOut << sroot.dump(4);
    sjsonOut.close();
    std::cout << "[Export] Saved: " << sjsonPath << "\n";

    WriteSceneBinary(
        sceneDir,
        sc.name,
        sl.roots,
        baseName,
        sl.nodes,
        sl.subMeshes,
        sl.matrixData,
        sl.trsPool
    );

    return true;
}

// ------------------------------------------------------------
// Public entry
// ------------------------------------------------------------
bool ExportPureModel(
    const gltf::Model            &model,
    const std::filesystem::path  &outDir
)
{
    pure::Model sm = pure::ConvertFromGLTF(model);

    std::filesystem::path baseDir = outDir.empty()
        ? std::filesystem::path(sm.gltf_source).parent_path()
        : outDir;
    std::error_code ec;
    std::filesystem::create_directories(baseDir, ec);

    const std::string baseName = stem_noext(sm.gltf_source);
    std::filesystem::path targetDir = baseDir / (baseName + ".StaticMesh");
    std::filesystem::create_directories(targetDir, ec);

    json root              = json::object();
    root["gltf_source"]    = sm.gltf_source;
    root["materials"]      = BuildMaterialsJson(sm);

    if (!WriteAllBoundsBinary(sm, targetDir))
        std::cerr << "[Export] Failed to write bounds binary." << "\n";
    root["bounds"] = static_cast<int64_t>(sm.bounds.size());

    root["scenes"]     = BuildScenesJson(sm);
    root["mesh_nodes"] = BuildMeshNodesJson(sm);

    WriteGlobalMatrices(root, sm, targetDir);
    WriteGlobalTRS(root, sm, targetDir);
    ExportGeometries(sm, targetDir);
    root["subMeshes"] = BuildSubMeshesJson(sm);

    std::filesystem::path jsonPath = targetDir / "StaticMesh.json";
    std::ofstream jsonOut(jsonPath, std::ios::binary);
    if (!jsonOut)
    {
        std::cerr << "[Export] Cannot open: " << jsonPath << "\n";
        return false;
    }
    jsonOut << root.dump(4);
    jsonOut.close();
    std::cout << "[Export] Saved: " << jsonPath << "\n";

    for (std::size_t si = 0; si < sm.scenes.size(); ++si)
    {
        if (!ExportScene(sm, si, targetDir, baseName))
            return false;
    }

    return true;
}

} // namespace exporters
