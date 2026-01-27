#include "gltf/import/SamplerMapping.h"

namespace gltf
{
    pure::WrapMode MapWrapModeFromGLTF(int v)
    {
        using namespace gl;
        switch (v) {
        case GL_CLAMP_TO_EDGE: return pure::WrapMode::ClampToEdge;
        case GL_MIRRORED_REPEAT: return pure::WrapMode::MirroredRepeat;
        case GL_REPEAT: default: return pure::WrapMode::Repeat;
        }
    }

    std::optional<pure::FilterMode> MapFilterModeFromGLTF(const std::optional<int> &v)
    {
        if (!v) return std::nullopt;
        using namespace gl;
        switch (*v) {
        case GL_NEAREST: return pure::FilterMode::Nearest;
        case GL_LINEAR: return pure::FilterMode::Linear;
        case GL_NEAREST_MIPMAP_NEAREST: return pure::FilterMode::NearestMipmapNearest;
        case GL_LINEAR_MIPMAP_NEAREST: return pure::FilterMode::LinearMipmapNearest;
        case GL_NEAREST_MIPMAP_LINEAR: return pure::FilterMode::NearestMipmapLinear;
        case GL_LINEAR_MIPMAP_LINEAR: return pure::FilterMode::LinearMipmapLinear;
        default: return std::nullopt;
        }
    }
}
