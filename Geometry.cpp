#include"Geometry.h"
#include<fstream>
#include<iostream>
#include<vector>
#include<cstdint>

namespace pure
{
    constexpr const char GeometryFileMagic[] = "Geometry\x1A";
    constexpr const size_t GeometryFileMagicBytes=sizeof(GeometryFileMagic)-1;

#pragma pack(push,1)
    struct GeometryFileHeader
    {
        uint16_t version;        // 1
        uint8_t  primitiveType;  // PrimitiveType as uint8_t
        uint32_t vertexCount;    // Number of vertices
        uint8_t  indexStride;    // 0 if no indices, otherwise 1,2,4
        uint32_t indexCount;     // Number of indices (0 if no indices)
        uint8_t  attributeCount; // Number of attributes
    };
#pragma pack(pop)

    bool SaveGeometry(const Geometry &geometry,const BoundingBox &bounds,const std::string &filename)
    {
        std::ofstream ofs(filename,std::ios::binary);

        if(!ofs)
        {
            std::cerr<<"Error: Cannot open file "<<filename<<" for writing."<<std::endl;
            return false;
        }

        ofs.write(GeometryFileMagic,GeometryFileMagicBytes);

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

        GeometryFileHeader header;

        header.version = 1;

        header.primitiveType    = static_cast<uint8_t >(geometry.primitiveType);
        header.vertexCount      = static_cast<uint32_t>(geometry.attributes.empty() ? 0 : geometry.attributes[0].count);
        header.indexStride      = index_stride;
        header.indexCount       = static_cast<uint32_t>(geometry.indices.has_value() ? geometry.indices->count : 0);
        header.attributeCount   = static_cast<uint8_t >(geometry.attributes.size());    //不可能有超过255个属性吧？

        ofs.write(reinterpret_cast<const char *>(&header),sizeof(GeometryFileHeader));

        // write BoundingBox (use external bounds parameter). Convert all values to float.
        Write(ofs,bounds);

        const size_t attributeCount = geometry.attributes.size();

        if(attributeCount>0)
        {
            if(attributeCount > 255)
            {
                std::cerr << "Error: Too many attributes (max 255)." << std::endl;
                return false;
            }

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

            ofs.write(reinterpret_cast<const char *>(attribute_format.data()),attributeCount);
            ofs.write(reinterpret_cast<const char *>(attribute_name_length.data()),attributeCount);

            uint8_t zero = 0;

            for(size_t i = 0;i < attributeCount;++i)
            {
                ofs.write(geometry.attributes[i].name.data(),attribute_name_length[i]);
                ofs.write(reinterpret_cast<const char *>(&zero),1);
            }

            for(const auto &attr : geometry.attributes)
            {
                uint32_t stride = GetStrideByFormat(attr.format);

                if(stride * header.vertexCount == attr.data.size())
                {
                    ofs.write(reinterpret_cast<const char *>(attr.data.data()),attr.data.size());
                }
                else
                {
                    std::cerr << "Error: Attribute data size mismatch in attribute " << attr.name << std::endl;
                    return false;
                }
            }
        }//if(attributeCount>0)

        if(header.indexCount>0)
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

            ofs.write(reinterpret_cast<const char *>(geometry.indicesData->data()),geometry.indicesData->size());
        }

        return true;
    }
}//namespace pure
