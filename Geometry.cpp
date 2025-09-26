#include"Geometry.h"
#include<fstream>
#include<iostream>

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
        uint32_t indexCount;     // Number of indices (0 if no indices)
        uint8_t  attributeCount; // Number of attributes
    };
#pragma pack(pop)

    bool SaveGeometry(const Geometry &geometry,const std::string &filename)
    {
        std::ofstream ofs(filename,std::ios::binary);

        if(!ofs)
        {
            std::cerr<<"Error: Cannot open file "<<filename<<" for writing."<<std::endl;
            return false;
        }

        ofs.write(GeometryFileMagic,GeometryFileMagicBytes);

        GeometryFileHeader header;

        header.version = 1;

        header.primitiveType    = static_cast<uint8_t >(geometry.primitiveType);
        header.vertexCount      = static_cast<uint32_t>(geometry.attributes.empty() ? 0 : geometry.attributes[0].count);
        header.indexCount       = static_cast<uint32_t>(geometry.indices.has_value() ? geometry.indices->count : 0);
        header.attributeCount   = static_cast<uint8_t >(geometry.attributes.size());

        ofs.write((char *)&header,sizeof(GeometryFileHeader));

        const uint8_t attributeCount = static_cast<uint8_t>(geometry.attributes.size());

        ofs.write((char *)&attributeCount,1);

        uint8_t *attribute_format = new uint8_t[attributeCount];
        uint8_t *attribute_name_length = new uint8_t[attributeCount];

        for(int i=0;i<attributeCount;++i)
        {
            attribute_format[i] = static_cast<uint8_t>(geometry.attributes[i].format);
            attribute_name_length[i] = static_cast<uint8_t>(geometry.attributes[i].name.size());

            if(geometry.attributes[i].count != header.vertexCount)
            {
                delete[] attribute_format;
                delete[] attribute_name_length;

                std::cerr<<"Error: Attribute count mismatch in attribute "<<geometry.attributes[i].name<<std::endl;
                return false;
            }
        }

        ofs.write(reinterpret_cast<const char *>(attribute_format),attributeCount);
        ofs.write(reinterpret_cast<const char *>(attribute_name_length),attributeCount);

        uint8_t zero=0;

        for(int i = 0;i < attributeCount;++i)
        {
            ofs.write(geometry.attributes[i].name.data(),attribute_name_length[i]);
            ofs.write((char *)&zero,1);
        }

        for(const auto &attr:geometry.attributes)
        {
            uint32_t stride = GetStrideByFormat(attr.format);

            if(stride*header.vertexCount==attr.data.size())
            {
                ofs.write((char *)attr.data.data(),attr.data.size());
            }
            else
            {
                delete[] attribute_format;
                delete[] attribute_name_length;
                std::cerr<<"Error: Attribute data size mismatch in attribute "<<attr.name<<std::endl;
                return false;
            }
        }

        if(header.indexCount>0)
        {
            ofs.write((char *)geometry.indicesData->data(),geometry.indicesData->size());
        }

        return true;
    }
}//namespace pure
