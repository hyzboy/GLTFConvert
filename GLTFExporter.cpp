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
#include <unordered_map>
#include <optional>

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

// Helper exporter class to organize the export steps
class Exporter {
public:
    Exporter(const std::filesystem::path& in, const std::filesystem::path& out) : inputPath(in), outDir(out) {}

    bool Run() {
        if (!std::filesystem::exists(inputPath)) {
            std::cerr << "[Export] Input not found: " << inputPath << "\n";
            return false;
        }

        if (!LoadAsset()) return false;
        PrepareDirectories();
        BuildNodeIndexMap();
        ComputeWorldMatrices();
        ExportNodes();
        ExtractPrimitives();
        ComputeSceneAABBs();
        WriteJson();
        return true;
    }

private:
    const std::filesystem::path inputPath;
    const std::filesystem::path outDir;

    std::filesystem::path target_dir;
    std::string base_name;

    fastgltf::Asset asset;

    // per-node world matrices
    std::vector<fastgltf::math::fmat4x4> world_mats;
    std::unordered_map<const fastgltf::Node*, size_t> node_ptr_to_index;

    // JSON pieces
    nlohmann::json root = nlohmann::json::object();
    nlohmann::json scenes_json = nlohmann::json::array();
    nlohmann::json nodes_json = nlohmann::json::array();
    nlohmann::json meshes_json = nlohmann::json::array();
    nlohmann::json global_primitives = nlohmann::json::array();

    // mapping mesh->global primitive indices
    std::vector<std::vector<int64_t>> mesh_prim_map;

    bool LoadAsset() {
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
        asset = std::move(asset_res.get());

        base_name = stem_noext(inputPath);
        return true;
    }

    void PrepareDirectories() {
        std::filesystem::path out_base_dir = outDir.empty() ? inputPath.parent_path() : outDir;
        std::error_code ec;
        std::filesystem::create_directories(out_base_dir, ec);
        target_dir = out_base_dir / (base_name + ".StaticMesh");
        std::filesystem::create_directories(target_dir, ec);

        root = nlohmann::json::object();
        root["gltf_source"] = std::filesystem::absolute(inputPath).string();

        // prepare scenes JSON (name + node indices)
        scenes_json = nlohmann::json::array();
        for (const auto &sc : asset.scenes) {
            nlohmann::json s = nlohmann::json::object();
            if (!sc.name.empty()) s["name"] = std::string(sc.name.data(), sc.name.size());
            nlohmann::json nodes = nlohmann::json::array();
            for (auto n : sc.nodeIndices) nodes.push_back(static_cast<int64_t>(n));
            s["nodes"] = std::move(nodes);
            scenes_json.push_back(std::move(s));
        }
    }

    void BuildNodeIndexMap() {
        world_mats.assign(asset.nodes.size(), fastgltf::math::fmat4x4());
        node_ptr_to_index.clear();
        for (size_t i = 0; i < asset.nodes.size(); ++i) node_ptr_to_index[&asset.nodes[i]] = i;
    }

    void ComputeWorldMatrices() {
        for (size_t si = 0; si < asset.scenes.size(); ++si) {
            fastgltf::iterateSceneNodes(asset, si, fastgltf::math::fmat4x4(), [&](fastgltf::Node& node, const fastgltf::math::fmat4x4& nodeMatrix) {
                auto it = node_ptr_to_index.find(&node);
                if (it != node_ptr_to_index.end()) {
                    world_mats[it->second] = nodeMatrix;
                }
            });
        }
    }

    void ExportNodes() {
        nodes_json = nlohmann::json::array();
        for (size_t ni = 0; ni < asset.nodes.size(); ++ni) {
            const auto &node = asset.nodes[ni];
            nlohmann::json nj = nlohmann::json::object();
            if (!node.name.empty()) nj["name"] = std::string(node.name.data(), node.name.size());
            if (node.meshIndex) nj["mesh"] = static_cast<int64_t>(*node.meshIndex);
            if (node.skinIndex) nj["skin"] = static_cast<int64_t>(*node.skinIndex);
            if (node.cameraIndex) nj["camera"] = static_cast<int64_t>(*node.cameraIndex);

            nlohmann::json child_arr = nlohmann::json::array();
            for (auto c : node.children) child_arr.push_back(static_cast<int64_t>(c));
            nj["children"] = std::move(child_arr);

            std::visit(fastgltf::visitor{
                [&](const fastgltf::TRS &trs) {
                    nlohmann::json t = nlohmann::json::object();
                    t["translation"] = nlohmann::json::array({ static_cast<double>(trs.translation.x()), static_cast<double>(trs.translation.y()), static_cast<double>(trs.translation.z()) });
                    t["rotation"] = nlohmann::json::array({ static_cast<double>(trs.rotation.x()), static_cast<double>(trs.rotation.y()), static_cast<double>(trs.rotation.z()), static_cast<double>(trs.rotation.w()) });
                    t["scale"] = nlohmann::json::array({ static_cast<double>(trs.scale.x()), static_cast<double>(trs.scale.y()), static_cast<double>(trs.scale.z()) });
                    nj["transform"] = std::move(t);
                },
                [&](const fastgltf::math::fmat4x4 &mat) {
                    const float* d = mat.data();
                    nlohmann::json arr = nlohmann::json::array();
                    for (size_t i = 0; i < 16; ++i) arr.push_back(static_cast<double>(d[i]));
                    nj["matrix"] = std::move(arr);
                }
            }, node.transform);

            // world matrix
            const auto &wm = world_mats[ni];
            const float* wd = wm.data();
            nlohmann::json warr = nlohmann::json::array();
            for (size_t i = 0; i < 16; ++i) warr.push_back(static_cast<double>(wd[i]));
            nj["world_matrix"] = std::move(warr);

            nodes_json.push_back(std::move(nj));
        }

        root["nodes"] = std::move(nodes_json);
    }

    void ExtractPrimitives() {
        global_primitives = nlohmann::json::array();
        meshes_json = nlohmann::json::array();

        mesh_prim_map.clear();
        mesh_prim_map.resize(asset.meshes.size());

        for (size_t mi = 0; mi < asset.meshes.size(); ++mi) {
            const auto &mesh = asset.meshes[mi];
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

                size_t position_accessor_index = static_cast<size_t>(-1);

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
                    auto aabb_opt = ComputeAABBFromAccessor(posAcc);
                    if (aabb_opt) {
                        const auto &mn = aabb_opt->first;
                        const auto &mx = aabb_opt->second;
                        nlohmann::json aabb = nlohmann::json::object();
                        aabb["min"] = nlohmann::json::array({mn[0], mn[1], mn[2]});
                        aabb["max"] = nlohmann::json::array({mx[0], mx[1], mx[2]});
                        p["aabb"] = std::move(aabb);
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

            // store mapping for this mesh
            for (auto &v : prim_indices) mesh_prim_map[mi].push_back(static_cast<int64_t>(v));

            m["primitives"] = std::move(prim_indices);
            meshes_json.push_back(std::move(m));
        }
    }

    void ComputeSceneAABBs() {
        for (size_t si = 0; si < asset.scenes.size(); ++si) {
            double sminx = std::numeric_limits<double>::infinity();
            double sminy = std::numeric_limits<double>::infinity();
            double sminz = std::numeric_limits<double>::infinity();
            double smaxx = -std::numeric_limits<double>::infinity();
            double smaxy = -std::numeric_limits<double>::infinity();
            double smaxz = -std::numeric_limits<double>::infinity();
            bool sany = false;

            fastgltf::iterateSceneNodes(asset, si, fastgltf::math::fmat4x4(), [&](fastgltf::Node& node, const fastgltf::math::fmat4x4& nodeMatrix) {
                if (!node.meshIndex) return;
                size_t meshIndex = *node.meshIndex;
                if (meshIndex >= mesh_prim_map.size()) return;

                for (size_t localPrimIdx = 0; localPrimIdx < asset.meshes[meshIndex].primitives.size(); ++localPrimIdx) {
                    if (localPrimIdx >= mesh_prim_map[meshIndex].size()) continue;
                    int64_t globalPrimIdx = mesh_prim_map[meshIndex][localPrimIdx];
                    if (globalPrimIdx < 0 || static_cast<size_t>(globalPrimIdx) >= global_primitives.size()) continue;

                    const auto &primJson = global_primitives[static_cast<size_t>(globalPrimIdx)];
                    if (!primJson.contains("aabb")) continue;
                    const auto &aabb = primJson["aabb"];
                    if (!aabb.contains("min") || !aabb.contains("max")) continue;
                    auto minArr = aabb["min"];
                    auto maxArr = aabb["max"];
                    if (minArr.size() < 3 || maxArr.size() < 3) continue;

                    double lx = minArr[0].get<double>();
                    double ly = minArr[1].get<double>();
                    double lz = minArr[2].get<double>();
                    double hx = maxArr[0].get<double>();
                    double hy = maxArr[1].get<double>();
                    double hz = maxArr[2].get<double>();

                    fastgltf::math::fmat4x4 wm = nodeMatrix;
                    auto transform_point = [&](double px, double py, double pz) {
                        fastgltf::math::fvec4 p(static_cast<float>(px), static_cast<float>(py), static_cast<float>(pz), 1.f);
                        auto tp = wm * p;
                        return std::array<double,3>{static_cast<double>(tp.x()), static_cast<double>(tp.y()), static_cast<double>(tp.z())};
                    };

                    std::array<double,3> corners[8] = {
                        transform_point(lx, ly, lz), transform_point(hx, ly, lz), transform_point(lx, hy, lz), transform_point(lx, ly, hz),
                        transform_point(hx, hy, lz), transform_point(hx, ly, hz), transform_point(lx, hy, hz), transform_point(hx, hy, hz)
                    };

                    for (auto &c : corners) {
                        sany = true;
                        sminx = std::min(sminx, c[0]); sminy = std::min(sminy, c[1]); sminz = std::min(sminz, c[2]);
                        smaxx = std::max(smaxx, c[0]); smaxy = std::max(smaxy, c[1]); smaxz = std::max(smaxz, c[2]);
                    }
                }
            });

            if (sany) {
                nlohmann::json sa = nlohmann::json::object();
                sa["min"] = nlohmann::json::array({sminx, sminy, sminz});
                sa["max"] = nlohmann::json::array({smaxx, smaxy, smaxz});
                if (si < scenes_json.size()) scenes_json[si]["aabb"] = std::move(sa);
            } else {
                if (si < scenes_json.size()) scenes_json[si]["aabb"] = nullptr;
            }
        }
    }

    void WriteJson() {
        root["scenes"] = std::move(scenes_json);
        root["meshes"] = std::move(meshes_json);
        root["primitives"] = std::move(global_primitives);

        std::filesystem::path json_path = target_dir / (base_name + ".json");
        std::ofstream json_out(json_path, std::ios::binary);
        if (!json_out) {
            std::cerr << "[Export] Cannot open: " << json_path << "\n";
            return;
        }
        json_out << root.dump(4);
        json_out.close();
        std::cout << "[Export] Saved: " << json_path << "\n";
    }

    // Compute AABB from an accessor (expected Vec3 of float or double). Returns min/max or nullopt.
    std::optional<std::pair<std::array<double,3>, std::array<double,3>>> ComputeAABBFromAccessor(const fastgltf::Accessor& posAcc) {
        if (posAcc.type != fastgltf::AccessorType::Vec3) return std::nullopt;

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
        } else {
            return std::nullopt;
        }

        if (!any) return std::nullopt;

        std::array<double,3> mn{minx, miny, minz};
        std::array<double,3> mx{maxx, maxy, maxz};
        return std::make_optional(std::make_pair(mn, mx));
    }
};

} // namespace

namespace gltf {

bool ExportToTOML(const std::filesystem::path& inputPath,
                  const std::filesystem::path& outDir)
{
    Exporter exp(inputPath, outDir);
    bool ok = exp.Run();
    if (ok) std::cout << "[Export] Done: " << inputPath << "\n";
    return ok;
}

} // namespace gltf
