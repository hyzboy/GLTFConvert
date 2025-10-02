#include"Geometry.h"
#include"mini_pack_builder.h"
#include<fstream>
#include<iostream>
#include<vector>
#include<cstdint>
#include<sstream>
#include<limits>

namespace pure
{
#pragma pack(push,1)
    struct GeometryHeader
    {
        uint16_t version;        // 1
        uint8_t  primitiveType;  // PrimitiveType as uint8_t
        uint32_t vertexCount;    // Number of vertices
        uint8_t  indexStride;    // 0 if no indices, otherwise 1,2,4
        uint32_t indexCount;     // Number of indices (0 if no indices)
        uint8_t  attributeCount; // Number of attributes
    };
    
    struct PackedBounds
    {
        float aabbMin[3];
        float aabbMax[3];
        float obbCenter[3];
        float obbAxisX[3];
        float obbAxisY[3];
        float obbAxisZ[3];
        float obbHalfSize[3];
        float sphereCenter[3];
        float sphereRadius;
    };
#pragma pack(pop)

    bool SaveGeometry(const Geometry &geometry,const BoundingBox &bounds,const std::string &filename)
    {
        // Build attribute metadata block only; header and bounds saved as separate entries
        uint8_t index_stride=0;

        if(geometry.indices.has_value())
        {
            if(geometry.indices->indexType==IndexType::U8 )index_stride=1;else
            if(geometry.indices->indexType==IndexType::U16)index_stride=2;else
            if(geometry.indices->indexType==IndexType::U32)index_stride=4;else
            {
                std::cerr<<"Error: Unsupported index type."<<std::endl;
                return false;
            }
        }

        GeometryHeader header;

        header.version = 1;

        header.primitiveType    = static_cast<uint8_t >(geometry.primitiveType);
        header.vertexCount      = static_cast<uint32_t>(geometry.attributes.empty() ? 0 : geometry.attributes[0].count);
        header.indexStride      = index_stride;
        header.indexCount       = static_cast<uint32_t>(geometry.indices.has_value() ? geometry.indices->count : 0);
        header.attributeCount   = static_cast<uint8_t >(geometry.attributes.size());    //不可能有超过255个属性吧？

        const size_t attributeCount = geometry.attributes.size();
        if(attributeCount>0)
        {
            if(attributeCount > 255)
            {
                std::cerr << "Error: Too many attributes (max 255)." << std::endl;
                return false;
            }
        }

        // Prepare attribute_meta bytes (attribute metadata only)
        std::vector<std::uint8_t> attribute_meta;

        // Attribute metadata: formats, name lengths, names with zero terminator
        if(attributeCount>0)
        {
            std::vector<uint8_t> attribute_format(attributeCount);
            std::vector<uint8_t> attribute_name_length(attributeCount);

            for(size_t i = 0;i < attributeCount;++i)
            {
                attribute_format[i] = static_cast<uint8_t>(geometry.attributes[i].format);
                attribute_name_length[i] = static_cast<uint8_t>(geometry.attributes[i].name.size());

                if(geometry.attributes[i].count != header.vertexCount)
                {
                    std::cerr << "Error: Attribute count mismatch in attribute " << geometry.attributes[i].name << std::endl;
                    return false;
                }
            }

            attribute_meta.insert(attribute_meta.end(), attribute_format.begin(), attribute_format.end());
            attribute_meta.insert(attribute_meta.end(), attribute_name_length.begin(), attribute_name_length.end());

            const uint8_t zero = 0;
            for(size_t i = 0;i < attributeCount;++i)
            {
                const std::string &nm = geometry.attributes[i].name;
                attribute_meta.insert(attribute_meta.end(), nm.begin(), nm.end());
                attribute_meta.push_back(zero);
            }
        }

        // Prepare pack builder
        MiniPackBuilder builder;
        std::string err;

        // Add header as its own entry using raw pointer API
        if(!builder.add_entry_from_buffer("GeometryHeader", &header, static_cast<std::uint32_t>(sizeof(GeometryHeader)), err))
        {
            std::cerr << "Error: add_entry_from_buffer(GeometryHeader) failed: " << err << std::endl;
            return false;
        }

        // Add attribute_meta as attribute metadata entry (raw pointer API)
        if(!attribute_meta.empty())
        {
            if(attribute_meta.size() > std::numeric_limits<std::uint32_t>::max())
            {
                std::cerr << "Error: AttributeMeta too large." << std::endl;
                return false;
            }
            if(!builder.add_entry_from_buffer("AttributeMeta", attribute_meta.data(), static_cast<std::uint32_t>(attribute_meta.size()), err))
            {
                std::cerr << "Error: add_entry_from_buffer(AttributeMeta) failed: " << err << std::endl;
                return false;
            }
        }
        else
        {
            // still add empty entry to keep structure predictable
            if(!builder.add_entry_from_buffer("AttributeMeta", nullptr, 0, err))
            {
                std::cerr << "Error: add_entry_from_buffer(AttributeMeta-empty) failed: " << err << std::endl;
                return false;
            }
        }

        // Add bounds as a tightly packed (1-byte aligned) struct entry named "Bounds" (raw pointer API)
        PackedBounds pb{};
        pb.aabbMin[0]=static_cast<float>(bounds.aabb.min.x); pb.aabbMin[1]=static_cast<float>(bounds.aabb.min.y); pb.aabbMin[2]=static_cast<float>(bounds.aabb.min.z);
        pb.aabbMax[0]=static_cast<float>(bounds.aabb.max.x); pb.aabbMax[1]=static_cast<float>(bounds.aabb.max.y); pb.aabbMax[2]=static_cast<float>(bounds.aabb.max.z);
        pb.obbCenter[0]=static_cast<float>(bounds.obb.center.x); pb.obbCenter[1]=static_cast<float>(bounds.obb.center.y); pb.obbCenter[2]=static_cast<float>(bounds.obb.center.z);
        pb.obbAxisX[0]=static_cast<float>(bounds.obb.axisX.x); pb.obbAxisX[1]=static_cast<float>(bounds.obb.axisX.y); pb.obbAxisX[2]=static_cast<float>(bounds.obb.axisX.z);
        pb.obbAxisY[0]=static_cast<float>(bounds.obb.axisY.x); pb.obbAxisY[1]=static_cast<float>(bounds.obb.axisY.y); pb.obbAxisY[2]=static_cast<float>(bounds.obb.axisY.z);
        pb.obbAxisZ[0]=static_cast<float>(bounds.obb.axisZ.x); pb.obbAxisZ[1]=static_cast<float>(bounds.obb.axisZ.y); pb.obbAxisZ[2]=static_cast<float>(bounds.obb.axisZ.z);
        pb.obbHalfSize[0]=static_cast<float>(bounds.obb.halfSize.x); pb.obbHalfSize[1]=static_cast<float>(bounds.obb.halfSize.y); pb.obbHalfSize[2]=static_cast<float>(bounds.obb.halfSize.z);
        pb.sphereCenter[0]=static_cast<float>(bounds.sphere.center.x); pb.sphereCenter[1]=static_cast<float>(bounds.sphere.center.y); pb.sphereCenter[2]=static_cast<float>(bounds.sphere.center.z);
        pb.sphereRadius=static_cast<float>(bounds.sphere.radius);

        if(!builder.add_entry_from_buffer("Bounds", &pb, static_cast<std::uint32_t>(sizeof(PackedBounds)), err))
        {
            std::cerr << "Error: add_entry_from_buffer(Bounds) failed: " << err << std::endl;
            return false;
        }

        // Add each attribute raw data as its own entry (raw pointer API, avoid vector copy)
        for(size_t i = 0; i < attributeCount; ++i)
        {
            const auto &attr = geometry.attributes[i];
            const uint32_t stride = GetStrideByFormat(attr.format);
            const size_t expectedSize = static_cast<size_t>(stride) * header.vertexCount;
            if(attr.data.size() != expectedSize)
            {
                std::cerr << "Error: Attribute data size mismatch in attribute " << attr.name << std::endl;
                return false;
            }

            if(attr.data.size() > std::numeric_limits<std::uint32_t>::max())
            {
                std::cerr << "Error: Attribute data too large in attribute " << attr.name << std::endl;
                return false;
            }

            if(!builder.add_entry_from_buffer(attr.name, attr.data.data(), static_cast<std::uint32_t>(attr.data.size()), err))
            {
                std::cerr << "Error: add_entry_from_buffer(" << attr.name << ") failed: " << err << std::endl;
                return false;
            }
        }

        // Add indices data as entry if present (raw pointer API)
        if(header.indexCount > 0)
        {
            if(!geometry.indicesData.has_value())
            {
                std::cerr<<"Error: Indices metadata present but indicesData missing."<<std::endl;
                return false;
            }

            if(geometry.indicesData->size()!=static_cast<size_t>(header.indexCount)*index_stride)
            {
                std::cerr<<"Error: Index data size mismatch."<<std::endl;
                return false;
            }

            if(geometry.indicesData->size() > std::numeric_limits<std::uint32_t>::max())
            {
                std::cerr<<"Error: Index data too large."<<std::endl;
                return false;
            }

            if(!builder.add_entry_from_buffer("indices", geometry.indicesData->data(), static_cast<std::uint32_t>(geometry.indicesData->size()), err))
            {
                std::cerr << "Error: add_entry_from_buffer(indices) failed: " << err << std::endl;
                return false;
            }
        }

        // Create file writer and build pack
        auto writer = create_file_writer(filename);
        if(!writer)
        {
            std::cerr<<"Error: Cannot open file "<<filename<<" for writing."<<std::endl;
            return false;
        }

        MiniPackBuildResult result;
        if(!builder.build_pack(writer.get(), /*index_only*/false, result, err))
        {
            std::cerr << "Error: build_pack failed: " << err << std::endl;
            return false;
        }

        return true;
    }
}//namespace pure
