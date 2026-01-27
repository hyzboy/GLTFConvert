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
                    p.geometry.attributes.emplace_back(std::move(a));
                }

                if(prim.indicesAccessor)
                {
                    const auto &acc=asset.accessors[*prim.indicesAccessor];
                    std::vector<std::byte> buf;
                    if(CopyAccessorToBytes(asset,acc,buf))
                    {
                        // Determine the max index value in the buffer according to original component type
                        uint64_t maxIndex = 0;
                        const size_t count = acc.count;
                        // Helper lambdas to read values
                        auto read_u32_from_bytes = [&](const std::byte *ptr)->uint32_t { uint32_t v; std::memcpy(&v, ptr, sizeof(uint32_t)); return v; };
                        auto read_u16_from_bytes = [&](const std::byte *ptr)->uint16_t { uint16_t v; std::memcpy(&v, ptr, sizeof(uint16_t)); return v; };
                        auto read_u8_from_bytes  = [&](const std::byte *ptr)->uint8_t  { uint8_t v;  std::memcpy(&v, ptr, sizeof(uint8_t));  return v; };

                        const auto compType = acc.componentType;
                        if(compType == fastgltf::ComponentType::UnsignedInt)
                        {
                            const uint32_t *src = reinterpret_cast<const uint32_t*>(buf.data());
                            for(size_t i=0;i<count;++i) maxIndex = std::max<uint64_t>(maxIndex, src[i]);
                        }
                        else if(compType == fastgltf::ComponentType::UnsignedShort)
                        {
                            const uint16_t *src = reinterpret_cast<const uint16_t*>(buf.data());
                            for(size_t i=0;i<count;++i) maxIndex = std::max<uint64_t>(maxIndex, src[i]);
                        }
                        else if(compType == fastgltf::ComponentType::UnsignedByte)
                        {
                            const uint8_t *src = reinterpret_cast<const uint8_t*>(buf.data());
                            for(size_t i=0;i<count;++i) maxIndex = std::max<uint64_t>(maxIndex, src[i]);
                        }
                        else
                        {
                            // Fallback: treat as u32
                            const uint32_t *src = reinterpret_cast<const uint32_t*>(buf.data());
                            for(size_t i=0;i<count;++i) maxIndex = std::max<uint64_t>(maxIndex, src[i]);
                        }

                        // Decide target index type based on maxIndex
                        if(maxIndex<=0xFF && GetAllowU8Indices())
                        {
                            // Can fit in U8 — use typed vector for clarity
                            std::vector<uint8_t> outU8;
                            outU8.resize(count);
                            for(size_t i=0;i<count;++i)
                            {
                                uint32_t v = 0;
                                if(compType == fastgltf::ComponentType::UnsignedInt) v = read_u32_from_bytes(buf.data() + i*sizeof(uint32_t));
                                else if(compType == fastgltf::ComponentType::UnsignedShort) v = read_u16_from_bytes(buf.data() + i*sizeof(uint16_t));
                                else if(compType == fastgltf::ComponentType::UnsignedByte) v = read_u8_from_bytes(buf.data() + i*sizeof(uint8_t));
                                else v = read_u32_from_bytes(buf.data() + i*sizeof(uint32_t));
                                outU8[i] = static_cast<uint8_t>(v);
                            }
                            // Assign into byte vector
                            const std::byte *bstart = reinterpret_cast<const std::byte*>(outU8.data());
                            p.geometry.indices = std::vector<std::byte>(bstart, bstart + outU8.size());
                            p.geometry.indexType = IndexType::U8;
                        }
                        else if(maxIndex<=0xFFFF)
                        {
                            // Can fit in U16 — use typed vector for clarity
                            std::vector<uint16_t> outU16;
                            outU16.resize(count);
                            for(size_t i=0;i<count;++i)
                            {
                                uint32_t v = 0;
                                if(compType == fastgltf::ComponentType::UnsignedInt) v = read_u32_from_bytes(buf.data() + i*sizeof(uint32_t));
                                else if(compType == fastgltf::ComponentType::UnsignedShort) v = read_u16_from_bytes(buf.data() + i*sizeof(uint16_t));
                                else if(compType == fastgltf::ComponentType::UnsignedByte) v = read_u8_from_bytes(buf.data() + i*sizeof(uint8_t));
                                else v = read_u32_from_bytes(buf.data() + i*sizeof(uint32_t));
                                outU16[i] = static_cast<uint16_t>(v);
                            }
                            const std::byte *bstart = reinterpret_cast<const std::byte*>(outU16.data());
                            const std::byte *bend = bstart + outU16.size() * sizeof(uint16_t);
                            p.geometry.indices = std::vector<std::byte>(bstart, bend);
                            p.geometry.indexType = IndexType::U16;
                        }
                        else
                        {
                            // Keep as u32 (original or fallback)
                            p.geometry.indices = std::move(buf);
                            p.geometry.indexType = FastGLTFComponentTypeToIndexType(acc.componentType);
                        }

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
