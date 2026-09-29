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

        /// 把 accessor 的数据拷成**紧凑**缓冲（每元素 elemSize 字节，元素间无填充）。
        ///
        /// glTF 允许一个 bufferView 里交错存放多个属性：此时 `byteStride > elemSize`，
        /// 且 POSITION 往往不从 view 起点开始（`accessor.byteOffset`）。**必须按 stride 取元素**——
        /// 按 `elemSize` 连续读会把邻居属性的字节当成顶点数据（实测 `BoxInterleaved` /
        /// `InterpolationTest` / `ClearCoatTest` / `AnisotropyStrengthTest` 等 7 个官方样本
        /// 顶点数据整体读错，世界 AABB 差 0.5~4.2）。索引 accessor 按 glTF 规范不带 stride，
        /// `value_or(elemSize)` 自然退化为紧凑读取。
        static bool CopyAccessorToBytes(const fastgltf::Asset &asset,
                                        const fastgltf::Accessor &accessor,
                                        std::vector<std::byte> &out)
        {
            if(!accessor.bufferViewIndex) return false;
            const auto &bv=asset.bufferViews[*accessor.bufferViewIndex];
            if(bv.bufferIndex>=asset.buffers.size()) return false;
            const auto &buf=asset.buffers[bv.bufferIndex];
            const size_t elemSize=fastgltf::getElementByteSize(accessor.type,accessor.componentType);
            const size_t stride=bv.byteStride.value_or(elemSize);
            const size_t totalBytes=elemSize*accessor.count;
            const size_t startByte=bv.byteOffset+accessor.byteOffset;
            // 交错时"最后一个元素"也要完整落在 buffer 内
            const size_t needed=accessor.count?(startByte+(accessor.count-1)*stride+elemSize):0;

            bool ok=false;
            auto copy_from=[&](const std::byte *base,size_t size) -> bool
            {
                if(needed>size) return false;
                out.resize(totalBytes);
                const std::byte *src=base+startByte;
                if(stride==elemSize)
                {
                    std::memcpy(out.data(),src,totalBytes);             // 紧凑：整段拷
                }
                else
                {
                    for(size_t i=0;i<accessor.count;++i)                // 交错：逐元素摘出来
                        std::memcpy(out.data()+i*elemSize,src+i*stride,elemSize);
                }
                return true;
            };
            std::visit(fastgltf::visitor{
                [&](const fastgltf::sources::Vector &vec)
                       {
                           ok=copy_from(vec.bytes.data(),vec.bytes.size());
                       },
                       [&](const fastgltf::sources::Array &arr)
                       {
                           ok=copy_from(arr.bytes.data(),arr.bytes.size());
                       },
                       [&](const fastgltf::sources::ByteView &bvw)
                       {
                           ok=copy_from(bvw.bytes.data(),bvw.bytes.size());
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
