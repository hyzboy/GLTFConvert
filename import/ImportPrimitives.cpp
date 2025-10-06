#include "ImportPrimitives.h"
#include <cstring>

namespace importers {
namespace {
    static bool CopyAccessorToBytes(const fastgltf::Asset& asset,
                                    const fastgltf::Accessor& accessor,
                                    std::vector<std::byte>& out)
    {
        if (!accessor.bufferViewIndex) return false;
        const auto& bv = asset.bufferViews[*accessor.bufferViewIndex];
        if (bv.bufferIndex >= asset.buffers.size()) return false;
        const auto& buf = asset.buffers[bv.bufferIndex];
        size_t elemSize = fastgltf::getElementByteSize(accessor.type, accessor.componentType);
        size_t totalBytes = elemSize * accessor.count;
        bool ok = false;
        std::visit(fastgltf::visitor{
            [&](const fastgltf::sources::Vector& vec){
                if (bv.byteOffset + accessor.byteOffset + totalBytes > vec.bytes.size()) return;
                const std::byte* src = vec.bytes.data() + bv.byteOffset + accessor.byteOffset;
                out.resize(totalBytes);
                std::memcpy(out.data(), src, totalBytes);
                ok = true;
            },
            [&](const fastgltf::sources::Array& arr){
                if (bv.byteOffset + accessor.byteOffset + totalBytes > arr.bytes.size()) return;
                const std::byte* src = arr.bytes.data() + bv.byteOffset + accessor.byteOffset;
                out.resize(totalBytes);
                std::memcpy(out.data(), src, totalBytes);
                ok = true;
            },
            [&](const fastgltf::sources::ByteView& bvw){
                if (bv.byteOffset + accessor.byteOffset + totalBytes > bvw.bytes.size()) return;
                const std::byte* src = bvw.bytes.data() + bv.byteOffset + accessor.byteOffset;
                out.resize(totalBytes);
                std::memcpy(out.data(), src, totalBytes);
                ok = true;
            },
            [&](const auto&){ }
        }, buf.data);
        return ok;
    }

    static std::optional<std::pair<glm::dvec3, glm::dvec3>> ComputeAABBFromAccessorFloat(const fastgltf::Asset& asset,const fastgltf::Accessor& acc)
    {
        if (acc.type != fastgltf::AccessorType::Vec3) return std::nullopt;
        if (acc.componentType != fastgltf::ComponentType::Float && acc.componentType != fastgltf::ComponentType::Double) return std::nullopt;
        glm::dvec3 mn(std::numeric_limits<double>::infinity());
        glm::dvec3 mx(-std::numeric_limits<double>::infinity());
        bool any = false;
        if (acc.componentType == fastgltf::ComponentType::Float) {
            fastgltf::iterateAccessor<fastgltf::math::fvec3>(asset, acc, [&](const fastgltf::math::fvec3& v){ any = true; mn = glm::min(mn, glm::dvec3(v.x(), v.y(), v.z())); mx = glm::max(mx, glm::dvec3(v.x(), v.y(), v.z())); });
        } else {
            fastgltf::iterateAccessor<fastgltf::math::dvec3>(asset, acc, [&](const fastgltf::math::dvec3& v){ any = true; mn = glm::min(mn, glm::dvec3(v.x(), v.y(), v.z())); mx = glm::max(mx, glm::dvec3(v.x(), v.y(), v.z())); });
        }
        if (!any) return std::nullopt; return std::make_pair(mn, mx);
    }
}

void ImportPrimitives(const fastgltf::Asset& asset, std::vector<GLTFPrimitive>& primitives)
{
    for (const auto& mesh : asset.meshes) {
        for (const auto& prim : mesh.primitives) {
            GLTFPrimitive p{};
            p.geometry.primitiveType = FastGLTFModeToPrimitiveType(prim.type);
            for (const auto& attr : prim.attributes) {
                const auto& acc = asset.accessors[attr.accessorIndex];
                GLTFGeometry::GLTFGeometryAttribute a{};
                a.name = attr.name;
                a.count = acc.count;
                a.format = FastGLTFAccessorTypeToVkFormat(acc.type, acc.componentType);
                a.accessorIndex = attr.accessorIndex;
                if (!CopyAccessorToBytes(asset, acc, a.data)) a.data.clear();
                p.geometry.attributes.emplace_back(std::move(a));
                if (p.geometry.attributes.back().name == "POSITION") {
                    if (auto mm = ComputeAABBFromAccessorFloat(asset, acc)) {
                        p.geometry.localAABB.min = mm->first;
                        p.geometry.localAABB.max = mm->second;
                    }
                }
            }
            if (prim.indicesAccessor) {
                const auto& acc = asset.accessors[*prim.indicesAccessor];
                std::vector<std::byte> buf;
                if (CopyAccessorToBytes(asset, acc, buf)) {
                    p.geometry.indices = std::move(buf);
                    p.geometry.indexCount = acc.count;
                    p.geometry.indicesAccessorIndex = prim.indicesAccessor;
                    p.geometry.indexType = FastGLTFComponentTypeToIndexType(acc.componentType);
                }
            }
            if (prim.materialIndex) p.material = *prim.materialIndex;
            primitives.emplace_back(std::move(p));
        }
    }
}

} // namespace importers
