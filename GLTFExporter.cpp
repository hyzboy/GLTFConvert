#include "GLTFExporter.h"

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <cstring>

namespace {

std::string stem_noext(const std::filesystem::path& p) {
    return p.stem().string();
}

bool CopyAccessorToBytes(const fastgltf::Asset& asset,
                         const fastgltf::Accessor& accessor,
                         std::vector<std::byte>& out)
{
    if (!accessor.bufferViewIndex)
        return false;

    const auto& bv = asset.bufferViews[*accessor.bufferViewIndex];
    if (bv.bufferIndex >= asset.buffers.size())
        return false;

    const auto& buf = asset.buffers[bv.bufferIndex];

    size_t elem_size = fastgltf::getElementByteSize(accessor.type, accessor.componentType);
    size_t total_bytes = elem_size * accessor.count;

    std::visit(fastgltf::visitor{
        [&](const fastgltf::sources::Vector& vec){
            if (bv.byteOffset + accessor.byteOffset + total_bytes > vec.bytes.size()) return;
            const std::byte* src = vec.bytes.data() + bv.byteOffset + accessor.byteOffset;
            out.resize(total_bytes);
            std::memcpy(reinterpret_cast<void*>(out.data()), reinterpret_cast<const void*>(src), total_bytes);
        },
        [&](const fastgltf::sources::Array& arr){
            if (bv.byteOffset + accessor.byteOffset + total_bytes > arr.bytes.size()) return;
            const std::byte* src = arr.bytes.data() + bv.byteOffset + accessor.byteOffset;
            out.resize(total_bytes);
            std::memcpy(reinterpret_cast<void*>(out.data()), reinterpret_cast<const void*>(src), total_bytes);
        },
        [&](const fastgltf::sources::ByteView& bvw){
            if (bv.byteOffset + accessor.byteOffset + total_bytes > bvw.bytes.size()) return;
            const std::byte* src = bvw.bytes.data() + bv.byteOffset + accessor.byteOffset;
            out.resize(total_bytes);
            std::memcpy(reinterpret_cast<void*>(out.data()), reinterpret_cast<const void*>(src), total_bytes);
        },
        [&](const auto&){ }
    }, buf.data);

    return !out.empty();
}

} // namespace

namespace gltf {

bool ExportToTOML(const std::filesystem::path& inputPath,
                  const std::filesystem::path& outDir)
{
    if (!std::filesystem::exists(inputPath)) {
        std::cerr << "[Export] Input not found: " << inputPath << "\n";
        return false;
    }

    fastgltf::Parser parser{};
    auto data_res = fastgltf::GltfDataBuffer::FromPath(inputPath);
    if (data_res.error() != fastgltf::Error::None) {
        std::cerr << "[Export] Read failed: " << fastgltf::getErrorMessage(data_res.error()) << "\n";
        return false;
    }
    auto data = std::move(data_res.get());

    constexpr fastgltf::Options options = fastgltf::Options::LoadExternalBuffers
                                        | fastgltf::Options::LoadGLBBuffers
                                        | fastgltf::Options::DecomposeNodeMatrices
                                        | fastgltf::Options::GenerateMeshIndices;

    auto parent = inputPath.parent_path();
    auto asset_res = parser.loadGltf(data, parent, options);
    if (asset_res.error() != fastgltf::Error::None) {
        std::cerr << "[Export] Parse failed: " << fastgltf::getErrorMessage(asset_res.error()) << "\n";
        return false;
    }
    fastgltf::Asset asset = std::move(asset_res.get());

    std::filesystem::path out_base_dir = outDir.empty() ? parent : outDir;
    std::error_code ec;
    std::filesystem::create_directories(out_base_dir, ec);
    const std::string base_name = stem_noext(inputPath);

    // Create target directory: <base_name>.StaticMesh
    std::filesystem::path target_dir = out_base_dir / (base_name + ".StaticMesh");
    std::filesystem::create_directories(target_dir, ec);

    nlohmann::json root = nlohmann::json::object();
    root["gltf_source"] = std::filesystem::absolute(inputPath).string();

    // Scenes
    nlohmann::json scenes = nlohmann::json::array();
    for (size_t si = 0; si < asset.scenes.size(); ++si) {
        const auto &sc = asset.scenes[si];
        nlohmann::json s = nlohmann::json::object();
        if (!sc.name.empty()) s["name"] = std::string(sc.name.data(), sc.name.size());
        nlohmann::json nodes = nlohmann::json::array();
        for (auto n : sc.nodeIndices) nodes.push_back(static_cast<int64_t>(n));
        s["nodes"] = std::move(nodes);
        scenes.push_back(std::move(s));
    }
    root["scenes"] = std::move(scenes);

    // Global primitives array (flattened)
    nlohmann::json global_primitives = nlohmann::json::array();

    // Meshes
    nlohmann::json meshes = nlohmann::json::array();
    for (size_t mi = 0; mi < asset.meshes.size(); ++mi) {
        const auto &mesh = asset.meshes[mi];
        nlohmann::json m = nlohmann::json::object();
        if (!mesh.name.empty()) m["name"] = std::string(mesh.name.data(), mesh.name.size());

        nlohmann::json prim_indices = nlohmann::json::array();

        for (size_t pi = 0; pi < mesh.primitives.size(); ++pi) {
            const auto &prim = mesh.primitives[pi];
            // compute global primitive index before creating files
            const int64_t prim_index = static_cast<int64_t>(global_primitives.size());

            nlohmann::json p = nlohmann::json::object();
            p["mode"] = static_cast<int64_t>(prim.type);
            if (prim.materialIndex) p["material"] = static_cast<int64_t>(*prim.materialIndex);
            if (prim.indicesAccessor) p["indices"] = static_cast<int64_t>(*prim.indicesAccessor);

            nlohmann::json attrs = nlohmann::json::array();
            for (const auto &attr : prim.attributes) {
                const std::string attr_name(attr.name.data(), attr.name.size());
                const size_t accessor_index = attr.accessorIndex;
                if (accessor_index >= asset.accessors.size()) continue;

                const auto &acc = asset.accessors[accessor_index];
                std::vector<std::byte> bytes;
                if (!CopyAccessorToBytes(asset, acc, bytes)) continue;

                std::filesystem::path bin_name = std::to_string(prim_index) + "." + attr_name;
                std::filesystem::path bin_path = target_dir / bin_name;

                std::ofstream ofs(bin_path, std::ios::binary);
                if (!ofs) {
                    std::cerr << "[Export] Write failed: " << bin_path << "\n";
                } else {
                    ofs.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
                    ofs.close();
                    std::cout << "[Export] Saved: " << bin_path << "\n";
                }

                nlohmann::json a = nlohmann::json::object();
                a["name"] = attr_name;
                a["accessor"] = static_cast<int64_t>(accessor_index);
                a["file"] = bin_name.string();
                a["count"] = static_cast<int64_t>(acc.count);
                a["componentType"] = static_cast<int64_t>(acc.componentType);
                a["type"] = static_cast<int64_t>(acc.type);
                attrs.push_back(std::move(a));
            }
            p["attributes"] = std::move(attrs);

            if (prim.indicesAccessor) {
                const auto &acc = asset.accessors[*prim.indicesAccessor];
                std::vector<std::byte> bytes;
                if (CopyAccessorToBytes(asset, acc, bytes)) {
                    std::filesystem::path bin_name = std::to_string(prim_index) + ".indices";
                    std::filesystem::path bin_path = target_dir / bin_name;
                    std::ofstream ofs(bin_path, std::ios::binary);
                    if (ofs) {
                        ofs.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
                        ofs.close();
                        std::cout << "[Export] Saved: " << bin_path << "\n";
                        p["indices_file"] = bin_name.string();
                    }
                }
            }

            // Append this primitive to global primitives and record its index
            prim_indices.push_back(prim_index);
            global_primitives.push_back(std::move(p));
        }

        m["primitives"] = std::move(prim_indices);
        meshes.push_back(std::move(m));
    }
    root["meshes"] = std::move(meshes);
    root["primitives"] = std::move(global_primitives);

    std::filesystem::path json_path = target_dir / (base_name + ".json");
    std::ofstream json_out(json_path, std::ios::binary);
    if (!json_out) {
        std::cerr << "[Export] Cannot open: " << json_path << "\n";
        return false;
    }
    json_out << root.dump(4);
    json_out.close();
    std::cout << "[Export] Saved: " << json_path << "\n";

    std::cout << "[Export] Done: " << json_path << "\n";
    return true;
}

} // namespace gltf
