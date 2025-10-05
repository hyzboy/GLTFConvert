#include "SceneBinary.h"

#include <fstream>
#include <iostream>
#include <vector>
#include <cstdint>

#include "mini_pack_builder.h"

namespace exporters {

#pragma pack(push,1)
    struct SceneHeader
    {
        uint32_t version;
        uint32_t rootCount;
        uint32_t matrixCount;
        uint32_t trsCount;
        uint32_t subMeshCount;
        uint32_t nodeCount;
    };
#pragma pack(pop)

    static inline void AppendU32(std::vector<uint8_t> &buf, uint32_t v){ buf.push_back(static_cast<uint8_t>(v & 0xFF)); buf.push_back(static_cast<uint8_t>((v>>8)&0xFF)); buf.push_back(static_cast<uint8_t>((v>>16)&0xFF)); buf.push_back(static_cast<uint8_t>((v>>24)&0xFF)); }
    static inline void AppendF32Vec(std::vector<uint8_t> &buf, const float *f, size_t count){ const uint8_t *p = reinterpret_cast<const uint8_t*>(f); buf.insert(buf.end(), p, p + sizeof(float)*count); }
    static inline void AppendBytes(std::vector<uint8_t> &buf, const void *data, size_t size){ const uint8_t *p = reinterpret_cast<const uint8_t*>(data); buf.insert(buf.end(), p, p + size); }

bool WriteSceneBinary(
    const std::filesystem::path& sceneDir,
    const std::string& sceneName,
    const std::vector<std::size_t>& sceneRootIndices,
    const std::string& baseName,
    const std::vector<pure::MeshNode>& nodes,
    const std::vector<pure::SubMesh>& subMeshes,
    const std::vector<pure::MatrixEntry>& matrices,
    const std::vector<pure::MeshNodeTransform>& trs)
{
    // Use sceneName + ".scene" as filename; fallback to "Scene.scene" if empty
    std::string fileName = sceneName.empty() ? std::string("Scene.scene") : (sceneName + ".scene");
    std::filesystem::path binPath = sceneDir / fileName;

    MiniPackBuilder builder;
    std::string err;

    // Header entry (no magic number)
    SceneHeader sh{};
    sh.version = 1;
    sh.rootCount = static_cast<uint32_t>(sceneRootIndices.size());
    sh.matrixCount = static_cast<uint32_t>(matrices.size());
    sh.trsCount = static_cast<uint32_t>(trs.size());
    sh.subMeshCount = static_cast<uint32_t>(subMeshes.size());
    sh.nodeCount = static_cast<uint32_t>(nodes.size());

    if(!builder.add_entry_from_buffer("Header", &sh, static_cast<uint32_t>(sizeof(SceneHeader)), err))
    {
        std::cerr << "[Export] Failed to add SceneHeader entry: " << err << "\n";
        return false;
    }

    // Scene name
    {
        // MiniPack entry already records its length; no need to prefix with an extra u32 length.
        std::vector<uint8_t> nameBuf;
        if(!sceneName.empty()) AppendBytes(nameBuf, sceneName.data(), sceneName.size());
        if(!builder.add_entry_from_buffer("Name", nameBuf.data(), static_cast<uint32_t>(nameBuf.size()), err))
        {
            std::cerr << "[Export] Failed to add SceneName entry: " << err << "\n";
            return false;
        }
    }

    // Roots
    {
        // Header.rootCount contains the number of roots; write raw u32 indices in sequence without a leading count.
        std::vector<uint8_t> rootsBuf;
        rootsBuf.reserve(sceneRootIndices.size() * 4);
        for (auto r : sceneRootIndices) AppendU32(rootsBuf, static_cast<uint32_t>(r));
        if(!builder.add_entry_from_buffer("Roots", rootsBuf.data(), static_cast<uint32_t>(rootsBuf.size()), err))
        {
            std::cerr << "[Export] Failed to add Roots entry: " << err << "\n";
            return false;
        }
    }

    // Matrices (local + world pairs)
    {
        std::vector<uint8_t> matsBuf;
        matsBuf.reserve(matrices.size() * sizeof(glm::mat4) * 2);
        for (const auto &m : matrices) {
            AppendBytes(matsBuf, &m.local, sizeof(glm::mat4));
            AppendBytes(matsBuf, &m.world, sizeof(glm::mat4));
        }
        if(!builder.add_entry_from_buffer("Matrices", matsBuf.data(), static_cast<uint32_t>(matsBuf.size()), err))
        {
            std::cerr << "[Export] Failed to add Matrices entry: " << err << "\n";
            return false;
        }
    }

    // TRS pool: tx,ty,tz, rx,ry,rz,rw, sx,sy,sz floats
    {
        std::vector<uint8_t> trsBuf;
        trsBuf.reserve(trs.size() * sizeof(float) * 10);
        for (const auto &t : trs) {
            float vals[10] = { static_cast<float>(t.translation.x), static_cast<float>(t.translation.y), static_cast<float>(t.translation.z),
                               static_cast<float>(t.rotation.x), static_cast<float>(t.rotation.y), static_cast<float>(t.rotation.z), static_cast<float>(t.rotation.w),
                               static_cast<float>(t.scale.x), static_cast<float>(t.scale.y), static_cast<float>(t.scale.z) };
            AppendBytes(trsBuf, vals, sizeof(vals));
        }
        if(!builder.add_entry_from_buffer("TRS", trsBuf.data(), static_cast<uint32_t>(trsBuf.size()), err))
        {
            std::cerr << "[Export] Failed to add TRS entry: " << err << "\n";
            return false;
        }
    }

    // SubMeshes: store geometry filenames as length-prefixed strings
    {
        std::vector<uint8_t> smBuf;
        for (const auto &sm : subMeshes) {
            std::string geomFile = baseName + "." + std::to_string(sm.geometry) + ".geometry";
            AppendU32(smBuf, static_cast<uint32_t>(geomFile.size()));
            if(!geomFile.empty()) AppendBytes(smBuf, geomFile.data(), geomFile.size());
        }
        if(!builder.add_entry_from_buffer("SubMeshes", smBuf.data(), static_cast<uint32_t>(smBuf.size()), err))
        {
            std::cerr << "[Export] Failed to add SubMeshes entry: " << err << "\n";
            return false;
        }
    }

    // Nodes: for each node write name (u32+bytes), matrixIndexPlusOne u32, trsIndexPlusOne u32, children count + list, subMeshes count + list
    {
        std::vector<uint8_t> nodesBuf;
        for (const auto &n : nodes) {
            AppendU32(nodesBuf, static_cast<uint32_t>(n.name.size()));
            if(!n.name.empty()) AppendBytes(nodesBuf, n.name.data(), n.name.size());
            AppendU32(nodesBuf, static_cast<uint32_t>(n.matrixIndexPlusOne));
            AppendU32(nodesBuf, static_cast<uint32_t>(n.trsIndexPlusOne));
            AppendU32(nodesBuf, static_cast<uint32_t>(n.children.size()));
            for (auto c : n.children) AppendU32(nodesBuf, static_cast<uint32_t>(c));
            AppendU32(nodesBuf, static_cast<uint32_t>(n.subMeshes.size()));
            for (auto sidx : n.subMeshes) AppendU32(nodesBuf, static_cast<uint32_t>(sidx));
        }
        if(!builder.add_entry_from_buffer("Nodes", nodesBuf.data(), static_cast<uint32_t>(nodesBuf.size()), err))
        {
            std::cerr << "[Export] Failed to add Nodes entry: " << err << "\n";
            return false;
        }
    }

    // Write pack to file
    {
        auto writer = create_file_writer(binPath.string());
        if(!writer)
        {
            std::cerr << "[Export] Cannot open scene binary for writing: " << binPath << "\n";
            return false;
        }
        MiniPackBuildResult result;
        if(!builder.build_pack(writer.get(), /*index_only*/false, result, err))
        {
            std::cerr << "[Export] Failed to build scene pack: " << err << "\n";
            return false;
        }
    }

    std::cout << "[Export] Saved: " << binPath << "\n";
    return true;
}

} // namespace exporters
