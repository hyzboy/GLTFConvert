#include "SceneBinary.h"

#include <fstream>
#include <iostream>

namespace exporters {

static inline void WriteU32(std::ofstream& ofs, uint32_t v){ ofs.write(reinterpret_cast<const char*>(&v), sizeof(v)); }
static inline void WriteF32(std::ofstream& ofs, float v){ ofs.write(reinterpret_cast<const char*>(&v), sizeof(v)); }
static inline void WriteMat4(std::ofstream& ofs, const glm::mat4& m){ ofs.write(reinterpret_cast<const char*>(&m), sizeof(glm::mat4)); }
static inline void WriteString(std::ofstream& ofs, const std::string& s){
    WriteU32(ofs, static_cast<uint32_t>(s.size()));
    if (!s.empty()) ofs.write(s.data(), s.size());
}
static inline void WriteVec3(std::ofstream& ofs, const glm::vec3& v){
    WriteF32(ofs, v.x); WriteF32(ofs, v.y); WriteF32(ofs, v.z);
}
static inline void WriteVec4(std::ofstream& ofs, const glm::vec4& v){
    WriteF32(ofs, v.x); WriteF32(ofs, v.y); WriteF32(ofs, v.z); WriteF32(ofs, v.w);
}
static inline void WriteVec4(std::ofstream& ofs, const glm::quat& q){
    WriteF32(ofs, q.x); WriteF32(ofs, q.y); WriteF32(ofs, q.z); WriteF32(ofs, q.w);
}

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
    std::filesystem::path binPath = sceneDir / "Scene.bin";
    std::ofstream ofs(binPath, std::ios::binary);
    if (!ofs) {
        std::cerr << "[Export] Cannot open scene binary: " << binPath << "\n";
        return false;
    }

    // Header
    const uint32_t magic = 0x454E4353; // 'SCNE' little-endian
    const uint32_t version = 1;
    ofs.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    ofs.write(reinterpret_cast<const char*>(&version), sizeof(version));

    // Scene name and roots
    WriteString(ofs, sceneName);
    WriteU32(ofs, static_cast<uint32_t>(sceneRootIndices.size()));
    for (auto r : sceneRootIndices) WriteU32(ofs, static_cast<uint32_t>(r));

    // Pools counts
    WriteU32(ofs, static_cast<uint32_t>(matrices.size()));
    WriteU32(ofs, static_cast<uint32_t>(trs.size()));
    WriteU32(ofs, static_cast<uint32_t>(subMeshes.size()));
    WriteU32(ofs, static_cast<uint32_t>(nodes.size()));

    // Matrices
    for (const auto& me : matrices) {
        WriteMat4(ofs, me.local);
        WriteMat4(ofs, me.world);
    }

    // TRS as 10 floats
    for (const auto& t : trs) {
        WriteVec3(ofs, t.translation);
        WriteVec4(ofs, t.rotation); // xyzw
        WriteVec3(ofs, t.scale);
    }

    // SubMeshes: only geometry filename string
    for (const auto& sm : subMeshes) {
        std::string geomFile = baseName + "." + std::to_string(sm.geometry) + ".geometry";
        WriteString(ofs, geomFile);
    }

    // Nodes
    for (const auto& n : nodes) {
        WriteString(ofs, n.name);
        WriteU32(ofs, static_cast<uint32_t>(n.matrixIndexPlusOne));
        WriteU32(ofs, static_cast<uint32_t>(n.trsIndexPlusOne));
        // children
        WriteU32(ofs, static_cast<uint32_t>(n.children.size()));
        for (auto c : n.children) WriteU32(ofs, static_cast<uint32_t>(c));
        // subMeshes indices (scene-local)
        WriteU32(ofs, static_cast<uint32_t>(n.subMeshes.size()));
        for (auto sidx : n.subMeshes) WriteU32(ofs, static_cast<uint32_t>(sidx));
    }

    ofs.close();
    std::cout << "[Export] Saved: " << binPath << "\n";
    return true;
}

} // namespace exporters
