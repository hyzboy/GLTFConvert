#include "FastGLTFConversions.h"

PrimitiveType FastGLTFModeToPrimitiveType(fastgltf::PrimitiveType t)
{
    switch(t)
    {
    case fastgltf::PrimitiveType::Points:        return PrimitiveType::Points;
    case fastgltf::PrimitiveType::Lines:         return PrimitiveType::Lines;
    case fastgltf::PrimitiveType::LineStrip:     return PrimitiveType::LineStrip;
    case fastgltf::PrimitiveType::Triangles:     return PrimitiveType::Triangles;
    case fastgltf::PrimitiveType::TriangleStrip: return PrimitiveType::TriangleStrip;
    case fastgltf::PrimitiveType::TriangleFan:   return PrimitiveType::Fan;
    default: return PrimitiveType::Triangles; // fallback
    }
}

IndexType FastGLTFComponentTypeToIndexType(fastgltf::ComponentType ct)
{
    switch(ct)
    {
    case fastgltf::ComponentType::UnsignedByte:  return IndexType::U8;
    case fastgltf::ComponentType::UnsignedShort: return IndexType::U16;
    case fastgltf::ComponentType::UnsignedInt:   return IndexType::U32;
    default: return IndexType::ERR;
    }
}

VkFormat FastGLTFAccessorTypeToVkFormat(fastgltf::AccessorType at, fastgltf::ComponentType ct)
{
    if(at==fastgltf::AccessorType::Scalar)
    {
        switch(ct)
        {
        case fastgltf::ComponentType::Byte:         return PF_R8I;
        case fastgltf::ComponentType::UnsignedByte: return PF_R8U;
        case fastgltf::ComponentType::Short:        return PF_R16I;
        case fastgltf::ComponentType::UnsignedShort:return PF_R16U;
        case fastgltf::ComponentType::Int:          return PF_R32I;
        case fastgltf::ComponentType::UnsignedInt:  return PF_R32U;
        case fastgltf::ComponentType::Float:        return PF_R32F;
        case fastgltf::ComponentType::Double:       return PF_R64F; // non-standard
        default: return PF_UNDEFINED;
        }
    }
    else if(at==fastgltf::AccessorType::Vec2)
    {
        switch(ct)
        {
        case fastgltf::ComponentType::Byte:         return PF_RG8I;
        case fastgltf::ComponentType::UnsignedByte: return PF_RG8U;
        case fastgltf::ComponentType::Short:        return PF_RG16I;
        case fastgltf::ComponentType::UnsignedShort:return PF_RG16U;
        case fastgltf::ComponentType::Int:          return PF_RG32I;
        case fastgltf::ComponentType::UnsignedInt:  return PF_RG32U;
        case fastgltf::ComponentType::Float:        return PF_RG32F;
        case fastgltf::ComponentType::Double:       return PF_RG64F; // non-standard
        default: return PF_UNDEFINED;
        }
    }
    else if(at==fastgltf::AccessorType::Vec3)
    {
        switch(ct)
        {
        case fastgltf::ComponentType::Byte:         return PF_RGB8I; // no GPU support,don't use
        case fastgltf::ComponentType::UnsignedByte: return PF_RGB8U; // no GPU support,don't use
        case fastgltf::ComponentType::Short:        return PF_RGB16I; // no GPU support,don't use
        case fastgltf::ComponentType::UnsignedShort:return PF_RGB16U; // no GPU support,don't use
        case fastgltf::ComponentType::Int:          return PF_RGB32I;
        case fastgltf::ComponentType::UnsignedInt:  return PF_RGB32U;
        case fastgltf::ComponentType::Float:        return PF_RGB32F;
        case fastgltf::ComponentType::Double:       return PF_RGB64F; // non-standard
        default: return PF_UNDEFINED;
        }
    }
    else if(at==fastgltf::AccessorType::Vec4)
    {
        switch(ct)
        {
        case fastgltf::ComponentType::Byte:         return PF_RGBA8I;
        case fastgltf::ComponentType::UnsignedByte: return PF_RGBA8U;
        case fastgltf::ComponentType::Short:        return PF_RGBA16I;
        case fastgltf::ComponentType::UnsignedShort:return PF_RGBA16U;
        case fastgltf::ComponentType::Int:          return PF_RGBA32I;
        case fastgltf::ComponentType::UnsignedInt:  return PF_RGBA32U;
        case fastgltf::ComponentType::Float:        return PF_RGBA32F;
        case fastgltf::ComponentType::Double:       return PF_RGBA64F; // non-standard
        default: return PF_UNDEFINED;
        }
    }

    return PF_UNDEFINED;
}
