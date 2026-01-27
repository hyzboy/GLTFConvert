#pragma once

#include <optional>
#include "pure/Sampler.h"

namespace gltf
{
    // Minimal OpenGL constant definitions (don't include gl.h)
    namespace gl
    {
        constexpr int GL_CLAMP_TO_EDGE = 33071;
        constexpr int GL_MIRRORED_REPEAT = 33648;
        constexpr int GL_REPEAT = 10497;

        constexpr int GL_NEAREST = 9728;
        constexpr int GL_LINEAR = 9729;
        constexpr int GL_NEAREST_MIPMAP_NEAREST = 9984;
        constexpr int GL_LINEAR_MIPMAP_NEAREST = 9985;
        constexpr int GL_NEAREST_MIPMAP_LINEAR = 9986;
        constexpr int GL_LINEAR_MIPMAP_LINEAR = 9987;
    }

    // Map integer glTF/OpenGL sampler values to pure enums
    pure::WrapMode MapWrapModeFromGLTF(int v);
    std::optional<pure::FilterMode> MapFilterModeFromGLTF(const std::optional<int> &v);
}
