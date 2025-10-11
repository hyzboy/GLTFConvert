#include "SamplerStrings.h"
#include <string>
#include "pure/Sampler.h"

namespace exporters
{
    std::string WrapModeToString(pure::WrapMode v)
    {
        switch(v)
        {
        case pure::WrapMode::ClampToEdge: return "CLAMP_TO_EDGE";
        case pure::WrapMode::MirroredRepeat: return "MIRRORED_REPEAT";
        case pure::WrapMode::Repeat: return "REPEAT";
        default: return "UNKNOWN";
        }
    }
    std::string FilterToString(pure::FilterMode v)
    {
        switch(v)
        {
        case pure::FilterMode::Nearest: return "NEAREST";
        case pure::FilterMode::Linear: return "LINEAR";
        case pure::FilterMode::NearestMipmapNearest: return "NEAREST_MIPMAP_NEAREST";
        case pure::FilterMode::LinearMipmapNearest: return "LINEAR_MIPMAP_NEAREST";
        case pure::FilterMode::NearestMipmapLinear: return "NEAREST_MIPMAP_LINEAR";
        case pure::FilterMode::LinearMipmapLinear: return "LINEAR_MIPMAP_LINEAR";
        default: return "UNKNOWN";
        }
    }
}
