#include "pure/Geometry.h"
#include "math/BoundingVolumes.h"
#include "mini_pack_builder.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <cstdint>
#include <sstream>
#include <limits>

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
#pragma pack(pop)

    namespace
    {
        bool get_index_stride(const Geometry &geometry,uint8_t &index_stride,std::string &err)
        {
            index_stride=0;
            if(geometry.indices.has_value())
            {
                if(geometry.indices->indexType==IndexType::U8) index_stride=1; else
                    if(geometry.indices->indexType==IndexType::U16) index_stride=2; else
                        if(geometry.indices->indexType==IndexType::U32) index_stride=4; else
                        {
                            err="Unsupported index type";
                            return false;
                        }
            }
            return true;
        }

        GeometryHeader make_header(const Geometry &geometry,uint8_t index_stride)
        {
            GeometryHeader h{};
            h.version=1;
            h.primitiveType=static_cast<uint8_t>(geometry.primitiveType);
            h.vertexCount=static_cast<uint32_t>(geometry.attributes.empty()?0:geometry.attributes[0].count);
            h.indexStride=index_stride;
            h.indexCount=static_cast<uint32_t>(geometry.indices.has_value()?geometry.indices->count:0);
            h.attributeCount=static_cast<uint8_t>(geometry.attributes.size());
            return h;
        }

        bool build_attribute_meta(const Geometry &geometry,const GeometryHeader &header,std::vector<std::uint8_t> &out,std::string &err)
        {
            out.clear();
            const size_t attributeCount=geometry.attributes.size();
            if(attributeCount==0)
            {
                return true; // empty ok
            }
            if(attributeCount>255)
            {
                err="Too many attributes (max 255)";
                return false;
            }

            std::vector<uint8_t> attribute_format(attributeCount);
            std::vector<uint8_t> attribute_name_length(attributeCount);

            for(size_t i=0;i<attributeCount;++i)
            {
                attribute_format[i]=static_cast<uint8_t>(geometry.attributes[i].format);
                attribute_name_length[i]=static_cast<uint8_t>(geometry.attributes[i].name.size());

                if(geometry.attributes[i].count!=header.vertexCount)
                {
                    err=std::string("Attribute count mismatch in attribute ")+geometry.attributes[i].name;
                    return false;
                }
            }

            out.insert(out.end(),attribute_format.begin(),attribute_format.end());
            out.insert(out.end(),attribute_name_length.begin(),attribute_name_length.end());

            const uint8_t zero=0;
            for(size_t i=0;i<attributeCount;++i)
            {
                const std::string &nm=geometry.attributes[i].name;
                out.insert(out.end(),nm.begin(),nm.end());
                out.push_back(zero);
            }
            return true;
        }

        bool add_header_entry(MiniPackBuilder &builder,const GeometryHeader &header,std::string &err)
        {
            return builder.add_entry_from_buffer("GeometryHeader",&header,static_cast<std::uint32_t>(sizeof(GeometryHeader)),err);
        }

        bool add_attribute_meta_entry(MiniPackBuilder &builder,const std::vector<std::uint8_t> &attribute_meta,std::string &err)
        {
            if(!attribute_meta.empty())
            {
                if(attribute_meta.size()>std::numeric_limits<std::uint32_t>::max())
                {
                    err="AttributeMeta too large";
                    return false;
                }
                return builder.add_entry_from_buffer("AttributeMeta",attribute_meta.data(),static_cast<std::uint32_t>(attribute_meta.size()),err);
            }
            else
            {
                return builder.add_entry_from_buffer("AttributeMeta",nullptr,0,err);
            }
        }

        bool add_bounds_entry(MiniPackBuilder &builder,const BoundingVolumes &volumes,std::string &err)
        {
            PackedBounds pb;

            volumes.Pack(&pb);

            return builder.add_entry_from_buffer("BoundingVolumes",&pb,static_cast<std::uint32_t>(sizeof(PackedBounds)),err);
        }

        bool add_attributes_entries(MiniPackBuilder &builder,const Geometry &geometry,const GeometryHeader &header,std::string &err)
        {
            for(size_t i=0; i<geometry.attributes.size(); ++i)
            {
                const auto &attr=geometry.attributes[i];
                const uint32_t stride=GetStrideByFormat(attr.format);
                const size_t expectedSize=static_cast<size_t>(stride)*header.vertexCount;
                if(attr.data.size()!=expectedSize)
                {
                    err=std::string("Attribute data size mismatch in attribute ")+attr.name;
                    return false;
                }
                if(attr.data.size()>std::numeric_limits<std::uint32_t>::max())
                {
                    err=std::string("Attribute data too large in attribute ")+attr.name;
                    return false;
                }
                if(!builder.add_entry_from_buffer(attr.name,attr.data.data(),static_cast<std::uint32_t>(attr.data.size()),err))
                {
                    return false;
                }
            }
            return true;
        }

        bool add_indices_entry(MiniPackBuilder &builder,const Geometry &geometry,uint8_t index_stride,std::string &err)
        {
            if(!geometry.indices.has_value())
            {
                return true;
            }
            const auto count=geometry.indices->count;
            if(!geometry.indicesData.has_value())
            {
                err="Indices metadata present but indicesData missing";
                return false;
            }
            if(geometry.indicesData->size()!=static_cast<size_t>(count)*index_stride)
            {
                err="Index data size mismatch";
                return false;
            }
            if(geometry.indicesData->size()>std::numeric_limits<std::uint32_t>::max())
            {
                err="Index data too large";
                return false;
            }
            return builder.add_entry_from_buffer("indices",geometry.indicesData->data(),static_cast<std::uint32_t>(geometry.indicesData->size()),err);
        }

        bool write_pack(MiniPackBuilder &builder,const std::string &filename,std::string &err)
        {
            auto writer=create_file_writer(filename);
            if(!writer)
            {
                err=std::string("Cannot open file for writing: ")+filename;
                return false;
            }
            MiniPackBuildResult result;
            if(!builder.build_pack(writer.get(), /*index_only*/false,result,err))
            {
                return false;
            }
            return true;
        }
    }

    bool SaveGeometry(const Geometry &geometry,const BoundingVolumes &volumes,const std::string &filename)
    {
        uint8_t index_stride=0;
        std::string err;
        if(!get_index_stride(geometry,index_stride,err))
        {
            std::cerr<<"Error: "<<err<<std::endl;
            return false;
        }

        GeometryHeader header=make_header(geometry,index_stride);

        std::vector<std::uint8_t> attribute_meta;
        if(!build_attribute_meta(geometry,header,attribute_meta,err))
        {
            std::cerr<<"Error: "<<err<<std::endl;
            return false;
        }

        MiniPackBuilder builder;
        if(!add_header_entry(builder,header,err))
        {
            std::cerr<<"Error: "<<err<<std::endl;
            return false;
        }
        if(!add_attribute_meta_entry(builder,attribute_meta,err))
        {
            std::cerr<<"Error: "<<err<<std::endl;
            return false;
        }
        if(!add_bounds_entry(builder,volumes,err))
        {
            std::cerr<<"Error: "<<err<<std::endl;
            return false;
        }
        if(!add_attributes_entries(builder,geometry,header,err))
        {
            std::cerr<<"Error: "<<err<<std::endl;
            return false;
        }
        if(!add_indices_entry(builder,geometry,index_stride,err))
        {
            std::cerr<<"Error: "<<err<<std::endl;
            return false;
        }

        if(!write_pack(builder,filename,err))
        {
            std::cerr<<"Error: "<<err<<std::endl;
            return false;
        }

        return true;
    }
}//namespace pure
