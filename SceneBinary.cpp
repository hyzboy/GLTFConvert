#include "SceneBinary.h"

#include <fstream>
#include <iostream>
#include <vector>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <algorithm>

#include "mini_pack_builder.h"

namespace exporters {

#pragma pack(push,1)
struct SceneHeader {
    uint32_t version;
    uint32_t rootCount;
    uint32_t matrixCount;   // number of glm::mat4 stored
    uint32_t trsCount;
    uint32_t subMeshCount;
    uint32_t nodeCount;
};
#pragma pack(pop)

static inline void AppendU32(std::vector<uint8_t>& buf, uint32_t v) { buf.push_back(static_cast<uint8_t>(v & 0xFF)); buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF)); buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF)); buf.push_back(static_cast<uint8_t>((v >> 24) & 0xFF)); }

static std::vector<std::string> g_nameList; static std::unordered_map<std::string,uint32_t> g_nameMap; static std::vector<uint32_t> g_subMeshNameIndices; static std::vector<uint32_t> g_nodeNameIndices;

static bool AddHeaderEntry(MiniPackBuilder& builder, const SceneHeader& sh, std::string& err) {
    if (!builder.add_entry_from_buffer("Header", &sh, static_cast<uint32_t>(sizeof(SceneHeader)), err)) { std::cerr << "[Export] Failed to add SceneHeader entry: " << err << "\n"; return false; } return true;
}

static bool BuildNameTableAndIndices(MiniPackBuilder& builder, const std::string& sceneName, const std::string& baseName, const std::vector<pure::SubMesh>& subMeshes, const std::vector<pure::MeshNode>& nodes, std::string& err) {
    g_nameList.clear(); g_nameMap.clear(); g_subMeshNameIndices.clear(); g_nodeNameIndices.clear();
    auto intern_name = [&](const std::string& s)->uint32_t { auto it = g_nameMap.find(s); if (it != g_nameMap.end()) return it->second; uint32_t idx = static_cast<uint32_t>(g_nameList.size()); g_nameList.push_back(s); g_nameMap.emplace(g_nameList.back(), idx); return idx; };
    intern_name(sceneName); g_subMeshNameIndices.reserve(subMeshes.size()); for (const auto& sm : subMeshes) { std::string geomFile = baseName + "." + std::to_string(sm.geometry); g_subMeshNameIndices.push_back(intern_name(geomFile)); }
    g_nodeNameIndices.reserve(nodes.size()); for (const auto& n : nodes) g_nodeNameIndices.push_back(intern_name(n.name));
    err.clear(); write_string_list(&builder, "NameTable", g_nameList, err); if (!err.empty()) { std::cerr << "[Export] Failed to add NameTable entry: " << err << "\n"; return false; } return true;
}

static bool AddRootsEntry(MiniPackBuilder& builder, const std::vector<int32_t>& sceneRootIndices, std::string& err) {
    if (!builder.add_entry_from_array<int32_t>("Roots", sceneRootIndices, err)) { std::cerr << "[Export] Failed to add Roots entry: " << err << "\n"; return false; } return true;
}

static bool AddMatrixData(MiniPackBuilder& builder, const std::vector<glm::mat4>& data, std::string& err) {
    if (!builder.add_entry_from_array<glm::mat4>("Matrices", data, err)) { std::cerr << "[Export] Failed to add Matrices entry: " << err << "\n"; return false; } return true;
}

static bool AddTRSEntry(MiniPackBuilder& builder, const std::vector<pure::MeshNodeTransform>& trs, std::string& err) {
    std::vector<float> flat; flat.reserve(trs.size() * 10); for (const auto& t : trs) { flat.push_back(t.translation.x); flat.push_back(t.translation.y); flat.push_back(t.translation.z); flat.push_back(t.rotation.x); flat.push_back(t.rotation.y); flat.push_back(t.rotation.z); flat.push_back(t.rotation.w); flat.push_back(t.scale.x); flat.push_back(t.scale.y); flat.push_back(t.scale.z); }
    if (!builder.add_entry_from_array<float>("TRS", flat, err)) { std::cerr << "[Export] Failed to add TRS entry: " << err << "\n"; return false; } return true;
}

static bool AddSubMeshesEntry(MiniPackBuilder& builder, std::string& err) {
    if (!builder.add_entry_from_array<uint32_t>("SubMeshes", g_subMeshNameIndices, err)) { std::cerr << "[Export] Failed to add SubMeshes entry: " << err << "\n"; return false; } return true;
}

static bool AddNodesEntry(MiniPackBuilder& builder, const std::vector<pure::MeshNode>& nodes, std::string& err) {
    std::vector<uint8_t> buf; buf.reserve(nodes.size() * 32);
    for (size_t i = 0; i < nodes.size(); ++i) {
        const auto& n = nodes[i];
        AppendU32(buf, g_nodeNameIndices[i]);
        AppendU32(buf, static_cast<uint32_t>(n.localMatrixIndexPlusOne));
        AppendU32(buf, static_cast<uint32_t>(n.worldMatrixIndexPlusOne));
        AppendU32(buf, static_cast<uint32_t>(n.trsIndexPlusOne));
        AppendU32(buf, static_cast<uint32_t>(n.children.size())); for (auto c : n.children) AppendU32(buf, static_cast<uint32_t>(c));
        AppendU32(buf, static_cast<uint32_t>(n.subMeshes.size())); for (auto s : n.subMeshes) AppendU32(buf, static_cast<uint32_t>(s));
    }
    if (!builder.add_entry_from_buffer("Nodes", buf.data(), static_cast<uint32_t>(buf.size()), err)) { std::cerr << "[Export] Failed to add Nodes entry: " << err << "\n"; return false; } return true;
}

bool WriteSceneBinary(const std::filesystem::path& sceneDir, const std::string& sceneName, const std::vector<int32_t>& sceneRootIndices, const std::string& baseName, const std::vector<pure::MeshNode>& nodes, const std::vector<pure::SubMesh>& subMeshes, const std::vector<glm::mat4>& matrixData, const std::vector<pure::MeshNodeTransform>& trs) {
    std::string fileName = sceneName.empty() ? std::string("Scene.scene") : (sceneName + ".scene"); std::filesystem::path binPath = sceneDir / fileName;
    MiniPackBuilder builder; std::string err; SceneHeader sh{}; sh.version = 1; sh.rootCount = static_cast<uint32_t>(sceneRootIndices.size()); sh.matrixCount = static_cast<uint32_t>(matrixData.size()); sh.trsCount = static_cast<uint32_t>(trs.size()); sh.subMeshCount = static_cast<uint32_t>(subMeshes.size()); sh.nodeCount = static_cast<uint32_t>(nodes.size());
    if (!AddHeaderEntry(builder, sh, err)) return false; if (!BuildNameTableAndIndices(builder, sceneName, baseName, subMeshes, nodes, err)) return false; if (!AddRootsEntry(builder, sceneRootIndices, err)) return false; if (!AddMatrixData(builder, matrixData, err)) return false; if (!AddTRSEntry(builder, trs, err)) return false; if (!AddSubMeshesEntry(builder, err)) return false; if (!AddNodesEntry(builder, nodes, err)) return false;
    auto writer = create_file_writer(binPath.string()); if (!writer) { std::cerr << "[Export] Cannot open scene binary for writing: " << binPath << "\n"; return false; } MiniPackBuildResult result; if (!builder.build_pack(writer.get(), false, result, err)) { std::cerr << "[Export] Failed to build scene pack: " << err << "\n"; return false; }
    std::cout << "[Export] Saved: " << binPath << "\n"; return true;
}

} // namespace exporters
