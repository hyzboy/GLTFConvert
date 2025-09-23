#include "GLTFExporter.h"
#include "VKFormat.h"

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <limits>

namespace {

std::string stem_noext(const std::filesystem::path& p) {
    return p.stem().string();
}

static std::string ComponentTypeToString(fastgltf::ComponentType ct) {
    using CT = fastgltf::ComponentType;
    switch (ct) {
        case CT::Byte: return "BYTE";
        case CT::UnsignedByte: return "UNSIGNED_BYTE";
        case CT::Short: return "SHORT";
        case CT::UnsignedShort: return "UNSIGNED_SHORT";
        case CT::Int: return "INT";
        case CT::UnsignedInt: return "UNSIGNED_INT";
        case CT::Float: return "FLOAT";
        case CT::Double: return "DOUBLE";
        default: return "UNKNOWN";
    }
}

static std::string AccessorTypeToString(fastgltf::AccessorType at) {
    using AT = fastgltf::AccessorType;
    switch (at) {
        case AT::Scalar: return "SCALAR";
        case AT::Vec2: return "VEC2";
        case AT::Vec3: return "VEC3";
        case AT::Vec4: return "VEC4";
        case AT::Mat2: return "MAT2";
        case AT::Mat3: return "MAT3";
        case AT::Mat4: return "MAT4";
        default: return "UNKNOWN";
    }
}

static std::string PrimitiveModeToString(int mode) {
    // glTF primitive mode integers: 0=POINTS,1=LINES,2=LINE_LOOP,3=LINE_STRIP,4=TRIANGLES,5=TRIANGLE_STRIP,6=TRIANGLE_FAN
    switch (mode) {
        case 0: return "POINTS";
        case 1: return "LINES";
        case 2: return "LINE_LOOP";
        case 3: return "LINE_STRIP";
        case 4: return "TRIANGLES";
        case 5: return "TRIANGLE_STRIP";
        case 6: return "TRIANGLE_FAN";
        default: return "UNKNOWN";
    }
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

    bool ok = false;
    std::visit(fastgltf::visitor{
        [&](const fastgltf::sources::Vector& vec){
            if (bv.byteOffset + accessor.byteOffset + total_bytes > vec.bytes.size()) return;
            const std::byte* src = vec.bytes.data() + bv.byteOffset + accessor.byteOffset;
            out.resize(total_bytes);
            std::memcpy(out.data(), src, total_bytes);
            ok = true;
        },
        [&](const fastgltf::sources::Array& arr){
            if (bv.byteOffset + accessor.byteOffset + total_bytes > arr.bytes.size()) return;
            const std::byte* src = arr.bytes.data() + bv.byteOffset + accessor.byteOffset;
            out.resize(total_bytes);
            std::memcpy(out.data(), src, total_bytes);
            ok = true;
        },
        [&](const fastgltf::sources::ByteView& bvw){
            if (bv.byteOffset + accessor.byteOffset + total_bytes > bvw.bytes.size()) return;
            const std::byte* src = bvw.bytes.data() + bv.byteOffset + accessor.byteOffset;
            out.resize(total_bytes);
            std::memcpy(out.data(), src, total_bytes);
            ok = true;
        },
        [&](const auto&){ }
    }, buf.data);

    return ok;
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
    for (const auto &sc : asset.scenes) {
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

    // Meshes: will reference global primitive indices
    nlohmann::json meshes = nlohmann::json::array();

    for (const auto &mesh : asset.meshes) {
        nlohmann::json m = nlohmann::json::object();
        if (!mesh.name.empty()) m["name"] = std::string(mesh.name.data(), mesh.name.size());

        nlohmann::json prim_indices = nlohmann::json::array();

        for (const auto &prim : mesh.primitives) {
            int64_t prim_index = static_cast<int64_t>(global_primitives.size());

            nlohmann::json p = nlohmann::json::object();
            p["mode"] = PrimitiveModeToString(static_cast<int>(prim.type));
            if (prim.materialIndex) p["material"] = static_cast<int64_t>(*prim.materialIndex);
            if (prim.indicesAccessor) p["indices"] = static_cast<int64_t>(*prim.indicesAccessor);

            nlohmann::json attrs = nlohmann::json::array();

            // remember POSITION accessor index if present
            size_t position_accessor_index = static_cast<size_t>(-1);

            // attributes
            for (const auto &attr : prim.attributes) {
                const std::string attr_name(attr.name.data(), attr.name.size());
                const size_t accessor_index = attr.accessorIndex;
                if (accessor_index >= asset.accessors.size()) continue;

                const auto &acc = asset.accessors[accessor_index];
                std::vector<std::byte> bytes;
                if (!CopyAccessorToBytes(asset, acc, bytes)) continue;

                // write binary file named "<prim_index>.<attributeName>" inside target_dir
                std::filesystem::path bin_name = std::to_string(prim_index) + "." + attr_name;
                std::filesystem::path bin_path = target_dir / bin_name;
                std::ofstream ofs(bin_path, std::ios::binary);
                if (ofs) {
                    ofs.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
                    ofs.close();
                    std::cout << "[Export] Saved: " << bin_path << "\n";
                } else {
                    std::cerr << "[Export] Write failed: " << bin_path << "\n";
                }

                if (attr_name == "POSITION") {
                    position_accessor_index = accessor_index;
                }

                nlohmann::json attr_j = nlohmann::json::object();
                attr_j["id"] = static_cast<int64_t>(attrs.size());
                attr_j["name"] = attr_name;
                attr_j["count"] = static_cast<int64_t>(acc.count);
                attr_j["componentType"] = ComponentTypeToString(acc.componentType);
                attr_j["type"] = AccessorTypeToString(acc.type);
                attrs.push_back(std::move(attr_j));
            }

            p["attributes"] = std::move(attrs);

            // compute AABB from POSITION accessor if available
            if (position_accessor_index != static_cast<size_t>(-1)) {
                const auto &posAcc = asset.accessors[position_accessor_index];
                // only handle VEC3 positions with float/double components
                if (posAcc.type == fastgltf::AccessorType::Vec3) {
                    double minx = std::numeric_limits<double>::infinity();
                    double miny = std::numeric_limits<double>::infinity();
                    double minz = std::numeric_limits<double>::infinity();
                    double maxx = -std::numeric_limits<double>::infinity();
                    double maxy = -std::numeric_limits<double>::infinity();
                    double maxz = -std::numeric_limits<double>::infinity();
                    bool any = false;

                    if (posAcc.componentType == fastgltf::ComponentType::Float) {
                        fastgltf::iterateAccessor<fastgltf::math::fvec3>(asset, posAcc, [&](const fastgltf::math::fvec3 &v){
                            any = true;
                            double x = static_cast<double>(v.x());
                            double y = static_cast<double>(v.y());
                            double z = static_cast<double>(v.z());
                            minx = std::min(minx, x); miny = std::min(miny, y); minz = std::min(minz, z);
                            maxx = std::max(maxx, x); maxy = std::max(maxy, y); maxz = std::max(maxz, z);
                        });
                    } else if (posAcc.componentType == fastgltf::ComponentType::Double) {
                        fastgltf::iterateAccessor<fastgltf::math::dvec3>(asset, posAcc, [&](const fastgltf::math::dvec3 &v){
                            any = true;
                            double x = v.x();
                            double y = v.y();
                            double z = v.z();
                            minx = std::min(minx, x); miny = std::min(miny, y); minz = std::min(minz, z);
                            maxx = std::max(maxx, x); maxy = std::max(maxy, y); maxz = std::max(maxz, z);
                        });
                    }

                    if (any) {
                        nlohmann::json aabb = nlohmann::json::object();
                        aabb["min"] = nlohmann::json::array({minx, miny, minz});
                        aabb["max"] = nlohmann::json::array({maxx, maxy, maxz});
                        p["aabb"] = std::move(aabb);
                    }
                }
            }

            // indices
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
                    } else {
                        std::cerr << "[Export] Write failed: " << bin_path << "\n";
                    }
                }
            }

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
