#include "Importer.h"

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <iostream>
#include <fstream>

using namespace puregltf;

namespace importers {

static std::string ModeToString(fastgltf::PrimitiveType t) {
    switch (t) {
        case fastgltf::PrimitiveType::Points: return "POINTS";
        case fastgltf::PrimitiveType::Lines: return "LINES";
        case fastgltf::PrimitiveType::LineLoop: return "LINE_LOOP";
        case fastgltf::PrimitiveType::LineStrip: return "LINE_STRIP";
        case fastgltf::PrimitiveType::Triangles: return "TRIANGLES";
        case fastgltf::PrimitiveType::TriangleStrip: return "TRIANGLE_STRIP";
        case fastgltf::PrimitiveType::TriangleFan: return "TRIANGLE_FAN";
        default: return "UNKNOWN";
    }
}

static std::string ComponentTypeToString(fastgltf::ComponentType ct) {
    switch (ct) {
        case fastgltf::ComponentType::Byte: return "BYTE";
        case fastgltf::ComponentType::UnsignedByte: return "UNSIGNED_BYTE";
        case fastgltf::ComponentType::Short: return "SHORT";
        case fastgltf::ComponentType::UnsignedShort: return "UNSIGNED_SHORT";
        case fastgltf::ComponentType::Int: return "INT";
        case fastgltf::ComponentType::UnsignedInt: return "UNSIGNED_INT";
        case fastgltf::ComponentType::Float: return "FLOAT";
        case fastgltf::ComponentType::Double: return "DOUBLE";
        default: return "UNKNOWN";
    }
}

static std::string AccessorTypeToString(fastgltf::AccessorType at) {
    switch (at) {
        case fastgltf::AccessorType::Scalar: return "SCALAR";
        case fastgltf::AccessorType::Vec2: return "VEC2";
        case fastgltf::AccessorType::Vec3: return "VEC3";
        case fastgltf::AccessorType::Vec4: return "VEC4";
        case fastgltf::AccessorType::Mat2: return "MAT2";
        case fastgltf::AccessorType::Mat3: return "MAT3";
        case fastgltf::AccessorType::Mat4: return "MAT4";
        default: return "UNKNOWN";
    }
}

static bool CopyAccessorToBytes(const fastgltf::Asset& asset, const fastgltf::Accessor& accessor, std::vector<std::byte>& out)
{
    if (!accessor.bufferViewIndex) return false;
    const auto& bv = asset.bufferViews[*accessor.bufferViewIndex];
    if (bv.bufferIndex >= asset.buffers.size()) return false;
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

static std::optional<std::pair<glm::dvec3, glm::dvec3>> ComputeAABBFromAccessorFloat(const fastgltf::Asset& asset, const fastgltf::Accessor& acc) {
    if (acc.type != fastgltf::AccessorType::Vec3) return std::nullopt;
    if (acc.componentType != fastgltf::ComponentType::Float && acc.componentType != fastgltf::ComponentType::Double)
        return std::nullopt;

    glm::dvec3 mn(std::numeric_limits<double>::infinity());
    glm::dvec3 mx(-std::numeric_limits<double>::infinity());
    bool any = false;
    if (acc.componentType == fastgltf::ComponentType::Float) {
        fastgltf::iterateAccessor<fastgltf::math::fvec3>(asset, acc, [&](const fastgltf::math::fvec3 &v){
            any = true;
            mn = glm::min(mn, glm::dvec3(v.x(), v.y(), v.z()));
            mx = glm::max(mx, glm::dvec3(v.x(), v.y(), v.z()));
        });
    } else {
        fastgltf::iterateAccessor<fastgltf::math::dvec3>(asset, acc, [&](const fastgltf::math::dvec3 &v){
            any = true;
            mn = glm::min(mn, glm::dvec3(v.x(), v.y(), v.z()));
            mx = glm::max(mx, glm::dvec3(v.x(), v.y(), v.z()));
        });
    }
    if (!any) return std::nullopt;
    return std::make_pair(mn, mx);
}

bool ImportFastGLTF(const std::filesystem::path& inputPath, puregltf::Model& outModel) {
    fastgltf::Parser parser{};
    auto data_res = fastgltf::GltfDataBuffer::FromPath(inputPath);
    if (data_res.error() != fastgltf::Error::None) {
        std::cerr << "[Import] Read failed: " << fastgltf::getErrorMessage(data_res.error()) << "\n";
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
        std::cerr << "[Import] Parse failed: " << fastgltf::getErrorMessage(asset_res.error()) << "\n";
        return false;
    }
    fastgltf::Asset asset = std::move(asset_res.get());

    outModel.source = std::filesystem::absolute(inputPath).string();

    // primitives
    for (const auto& mesh : asset.meshes) {
        for (const auto& prim : mesh.primitives) {
            Primitive p{};
            p.mode = ModeToString(prim.type);

            // attributes
            for (const auto& attr : prim.attributes) {
                const std::string name(attr.name.data(), attr.name.size());
                const auto& acc = asset.accessors[attr.accessorIndex];
                Primitive::Attribute a{};
                a.name = name;
                a.count = acc.count;
                a.componentType = ComponentTypeToString(acc.componentType);
                a.type = AccessorTypeToString(acc.type);
                if (!CopyAccessorToBytes(asset, acc, a.data)) {
                    a.data.clear();
                }
                p.attributes.emplace_back(std::move(a));

                if (name == "POSITION") {
                    if (auto mm = ComputeAABBFromAccessorFloat(asset, acc)) {
                        p.localAABB.min = mm->first;
                        p.localAABB.max = mm->second;
                    }
                }
            }

            // indices
            if (prim.indicesAccessor) {
                const auto& acc = asset.accessors[*prim.indicesAccessor];
                std::vector<std::byte> buf;
                if (CopyAccessorToBytes(asset, acc, buf)) {
                    p.indices = std::move(buf);
                }
            }

            outModel.primitives.emplace_back(std::move(p));
        }
    }

    // meshes -> indices into primitives
    std::size_t primCursor = 0;
    for (const auto& mesh : asset.meshes) {
        Mesh m{};
        if (!mesh.name.empty()) m.name.assign(mesh.name.begin(), mesh.name.end());
        for (std::size_t i = 0; i < mesh.primitives.size(); ++i) {
            m.primitives.push_back(primCursor++);
        }
        outModel.meshes.emplace_back(std::move(m));
    }

    // nodes
    outModel.nodes.resize(asset.nodes.size());
    for (std::size_t i = 0; i < asset.nodes.size(); ++i) {
        const auto& n = asset.nodes[i];
        auto& on = outModel.nodes[i];
        if (!n.name.empty()) on.name.assign(n.name.begin(), n.name.end());
        if (n.meshIndex) on.mesh = *n.meshIndex;
        on.children.assign(n.children.begin(), n.children.end());
        if (std::holds_alternative<fastgltf::TRS>(n.transform)) {
            const auto& trs = std::get<fastgltf::TRS>(n.transform);
            on.hasMatrix = false;
            on.translation = glm::dvec3(trs.translation.x(), trs.translation.y(), trs.translation.z());
            on.rotation = glm::dquat(trs.rotation.w(), trs.rotation.x(), trs.rotation.y(), trs.rotation.z());
            on.scale = glm::dvec3(trs.scale.x(), trs.scale.y(), trs.scale.z());
        } else {
            const auto& mat = std::get<fastgltf::math::fmat4x4>(n.transform);
            glm::dmat4 m(1.0);
            const float* d = mat.data();
            // column-major fill
            for (int c = 0; c < 4; ++c)
                for (int r = 0; r < 4; ++r)
                    m[c][r] = static_cast<double>(d[c * 4 + r]);
            on.hasMatrix = true;
            on.matrix = m;
        }
    }

    // scenes
    outModel.scenes.resize(asset.scenes.size());
    for (std::size_t i = 0; i < asset.scenes.size(); ++i) {
        const auto& sc = asset.scenes[i];
        auto& os = outModel.scenes[i];
        if (!sc.name.empty()) os.name.assign(sc.name.begin(), sc.name.end());
        os.nodes.assign(sc.nodeIndices.begin(), sc.nodeIndices.end());
    }

    // compute derived data
    outModel.convertToZUp();

    return true;
}

} // namespace importers
