#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <vector>
#include <optional>
#include <limits>
#include <cstring>
#include <algorithm>
#include <cstdint>

#include "gltf/GLTFPrimitive.h"
#include "common/FastGLTFConversions.h"
#include "gltf/import/GLTFImporter.h"

namespace gltf
{
    namespace
    {
        static void EnsureTangentHasW(GLTFGeometry::GLTFGeometryAttribute &a)
        {
            if(a.name != "TANGENT" || a.data.empty())
                return;

            if(a.format == PF_RGB32F)
            {
                const size_t count = a.count;
                const float *src = reinterpret_cast<const float *>(a.data.data());
                std::vector<std::byte> out(count * sizeof(float) * 4);
                float *dst = reinterpret_cast<float *>(out.data());

                for(size_t i = 0; i < count; ++i)
                {
                    dst[i * 4 + 0] = src[i * 3 + 0];
                    dst[i * 4 + 1] = src[i * 3 + 1];
                    dst[i * 4 + 2] = src[i * 3 + 2];
                    dst[i * 4 + 3] = 1.0f; // default tangent.w = +1; TODO: compute handedness when tangent basis data is available
                }

                a.data = std::move(out);
                a.format = PF_RGBA32F;
            }
            else
            if(a.format == PF_RGB64F)
            {
                const size_t count = a.count;
                const double *src = reinterpret_cast<const double *>(a.data.data());
                std::vector<std::byte> out(count * sizeof(double) * 4);
                double *dst = reinterpret_cast<double *>(out.data());

                for(size_t i = 0; i < count; ++i)
                {
                    dst[i * 4 + 0] = src[i * 3 + 0];
                    dst[i * 4 + 1] = src[i * 3 + 1];
                    dst[i * 4 + 2] = src[i * 3 + 2];
                    dst[i * 4 + 3] = 1.0; // default tangent.w = +1; TODO: compute handedness when tangent basis data is available
                }

                a.data = std::move(out);
                a.format = PF_RGBA64F;
            }
        }

        static bool CopyAccessorToBytes(const fastgltf::Asset &asset,
                                        const fastgltf::Accessor &accessor,
                                        std::vector<std::byte> &out)
        {
            if(!accessor.bufferViewIndex) return false;
            const auto &bv=asset.bufferViews[*accessor.bufferViewIndex];
            if(bv.bufferIndex>=asset.buffers.size()) return false;
            const auto &buf=asset.buffers[bv.bufferIndex];
            size_t elemSize=fastgltf::getElementByteSize(accessor.type,accessor.componentType);
            size_t totalBytes=elemSize*accessor.count;
            bool ok=false;
            std::visit(fastgltf::visitor{
                [&](const fastgltf::sources::Vector &vec)
                       {
                           if(bv.byteOffset+accessor.byteOffset+totalBytes>vec.bytes.size()) return;
                           const std::byte *src=vec.bytes.data()+bv.byteOffset+accessor.byteOffset;
                           out.resize(totalBytes);
                           std::memcpy(out.data(),src,totalBytes);
                           ok=true;
                       },
                       [&](const fastgltf::sources::Array &arr)
                       {
                           if(bv.byteOffset+accessor.byteOffset+totalBytes>arr.bytes.size()) return;
                           const std::byte *src=arr.bytes.data()+bv.byteOffset+accessor.byteOffset;
                           out.resize(totalBytes);
                           std::memcpy(out.data(),src,totalBytes);
                           ok=true;
                       },
                       [&](const fastgltf::sources::ByteView &bvw)
                       {
                           if(bv.byteOffset+accessor.byteOffset+totalBytes>bvw.bytes.size()) return;
                           const std::byte *src=bvw.bytes.data()+bv.byteOffset+accessor.byteOffset;
                           out.resize(totalBytes);
                           std::memcpy(out.data(),src,totalBytes);
                           ok=true;
                       },
                       [&](const auto &) {}
                       },buf.data);
            return ok;
        }
    }

    void ImportPrimitives(const fastgltf::Asset &asset,std::vector<GLTFPrimitive> &primitives)
    {
        for(const auto &mesh:asset.meshes)
        {
            for(const auto &prim:mesh.primitives)
            {
                GLTFPrimitive p{};
                p.geometry.primitiveType=FastGLTFModeToPrimitiveType(prim.type);
                for(const auto &attr:prim.attributes)
                {
                    const auto &acc=asset.accessors[attr.accessorIndex];
                    GLTFGeometry::GLTFGeometryAttribute a{};
                    a.name=attr.name;
                    a.count=acc.count;
                    a.format=FastGLTFAccessorTypeToVkFormat(acc.type,acc.componentType);
                    a.accessorIndex=attr.accessorIndex;
                    if(!CopyAccessorToBytes(asset,acc,a.data)) a.data.clear();
                    EnsureTangentHasW(a);
                    p.geometry.attributes.emplace_back(std::move(a));
                }

                if(prim.indicesAccessor)
                {
                    const auto &acc=asset.accessors[*prim.indicesAccessor];
                    std::vector<std::byte> buf;
                    if(CopyAccessorToBytes(asset,acc,buf))
                    {
                        // Determine the max index value in the buffer according to original component type
                        const size_t count = acc.count;
                        // Helper lambdas to read values
                        auto read_u32_from_bytes = [&](const std::byte *ptr)->uint32_t { uint32_t v; std::memcpy(&v, ptr, sizeof(uint32_t)); return v; };
                        auto read_u16_from_bytes = [&](const std::byte *ptr)->uint16_t { uint16_t v; std::memcpy(&v, ptr, sizeof(uint16_t)); return v; };
                        auto read_u8_from_bytes  = [&](const std::byte *ptr)->uint8_t  { uint8_t v;  std::memcpy(&v, ptr, sizeof(uint8_t));  return v; };

                        const auto compType = acc.componentType;

                        // 引擎统一 uint32 索引（U8/U16 已废弃——按原始 stride 读取后展开为 uint32）
                        std::vector<uint32_t> outU32;
                        outU32.resize(count);
                        for(size_t i=0;i<count;++i)
                        {
                            uint32_t v = 0;
                            if(compType == fastgltf::ComponentType::UnsignedInt) v = read_u32_from_bytes(buf.data() + i*sizeof(uint32_t));
                            else if(compType == fastgltf::ComponentType::UnsignedShort) v = read_u16_from_bytes(buf.data() + i*sizeof(uint16_t));
                            else if(compType == fastgltf::ComponentType::UnsignedByte) v = read_u8_from_bytes(buf.data() + i*sizeof(uint8_t));
                            else v = read_u32_from_bytes(buf.data() + i*sizeof(uint32_t));
                            outU32[i] = v;
                        }
                        const std::byte *bstart = reinterpret_cast<const std::byte*>(outU32.data());
                        const std::byte *bend = bstart + outU32.size() * sizeof(uint32_t);
                        p.geometry.indices = std::vector<std::byte>(bstart, bend);
                        p.geometry.indexType = IndexType::U32;

                        p.geometry.indexCount=acc.count;
                        p.geometry.indicesAccessorIndex=prim.indicesAccessor;
                    }
                }
                if(prim.materialIndex) p.material=*prim.materialIndex;
                primitives.emplace_back(std::move(p));
            }
        }
    }
} // namespace gltf
