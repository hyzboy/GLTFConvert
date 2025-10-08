#include "SamplerStrings.h"
#include <string>

namespace exporters
{
    std::string WrapModeToString(int v)
    {
        switch(v)
        {
        case 33071: return "CLAMP_TO_EDGE";      // GL_CLAMP_TO_EDGE
        case 33648: return "MIRRORED_REPEAT";   // GL_MIRRORED_REPEAT
        case 10497: return "REPEAT";            // GL_REPEAT
        default:    return std::to_string(v);
        }
    }
    std::string FilterToString(int v)
    {
        switch(v)
        {
        case 9728: return "NEAREST";                 // GL_NEAREST
        case 9729: return "LINEAR";                  // GL_LINEAR
        case 9984: return "NEAREST_MIPMAP_NEAREST";  // GL_NEAREST_MIPMAP_NEAREST
        case 9985: return "LINEAR_MIPMAP_NEAREST";   // GL_LINEAR_MIPMAP_NEAREST
        case 9986: return "NEAREST_MIPMAP_LINEAR";   // GL_NEAREST_MIPMAP_LINEAR
        case 9987: return "LINEAR_MIPMAP_LINEAR";    // GL_LINEAR_MIPMAP_LINEAR
        default:   return std::to_string(v);
        }
    }
}
