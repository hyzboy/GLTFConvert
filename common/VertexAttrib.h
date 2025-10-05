#pragma once

#include<stdint.h>

#pragma pack(push,1)
enum class VertexAttribBaseType
{
    Bool=0,
    Int,
    UInt,
    Float,
    Double,
};//enum class VertexAttribBaseType

using VABaseType=VertexAttribBaseType;

struct VertexAttribType
{
    union
    {
        struct
        {
            VertexAttribBaseType basetype:4;
            unsigned int vec_size:4;
        };

        uint8_t vat_code;
    };

public:

    const bool Check()const
    {
        if(basetype<VertexAttribBaseType::Bool
            ||basetype>VertexAttribBaseType::Double)return(false);

        if(vec_size<=0||vec_size>4)return(false);

        return(true);
    }

    int Comp(const VertexAttribType &vat)const
    {
        int off=(int)basetype-(int)vat.basetype;
    
        if(off)return(off);

        off=vec_size-vat.vec_size;

        return(off);
    }

    const bool operator > (const VertexAttribType& i)const {return Comp(i)>0;}
    const bool operator < (const VertexAttribType& i)const {return Comp(i)<0;}
    const bool operator >=(const VertexAttribType& i)const {return Comp(i) >= 0;}
    const bool operator <=(const VertexAttribType& i)const {return Comp(i) <= 0;}
    const bool operator ==(const VertexAttribType& i)const {return Comp(i) == 0;}
    const bool operator !=(const VertexAttribType& i)const {return Comp(i) != 0;}

    const uint8_t ToCode()const{return vat_code;}

    const bool FromCode(const uint8_t code)
    {
        vat_code=code;

        return Check();
    }
};//struct VertexAttribType
#pragma pack(pop)

using VAType=VertexAttribType;

constexpr const VAType VAT_BOOL ={VABaseType::Bool,1};
constexpr const VAType VAT_BVEC2={VABaseType::Bool,2};
constexpr const VAType VAT_BVEC3={VABaseType::Bool,3};
constexpr const VAType VAT_BVEC4={VABaseType::Bool,4};

constexpr const VAType VAT_INT  ={VABaseType::Int,1};
constexpr const VAType VAT_IVEC2={VABaseType::Int,2};
constexpr const VAType VAT_IVEC3={VABaseType::Int,3};
constexpr const VAType VAT_IVEC4={VABaseType::Int,4};

constexpr const VAType VAT_UINT ={VABaseType::UInt,1};
constexpr const VAType VAT_UVEC2={VABaseType::UInt,2};
constexpr const VAType VAT_UVEC3={VABaseType::UInt,3};
constexpr const VAType VAT_UVEC4={VABaseType::UInt,4};

constexpr const VAType VAT_FLOAT={VABaseType::Float,1};
constexpr const VAType VAT_VEC2 ={VABaseType::Float,2};
constexpr const VAType VAT_VEC3 ={VABaseType::Float,3};
constexpr const VAType VAT_VEC4 ={VABaseType::Float,4};

constexpr const VAType VAT_DOUBLE={VABaseType::Double,1};
constexpr const VAType VAT_DVEC2 ={VABaseType::Double,2};
constexpr const VAType VAT_DVEC3 ={VABaseType::Double,3};
constexpr const VAType VAT_DVEC4 ={VABaseType::Double,4};

constexpr const size_t VERTEX_ATTRIB_NAME_MAX_LENGTH=32;
