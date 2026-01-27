#pragma once

#include <string>
#include <optional>
#include <cstddef>

// 表示 glTF 的 texture 节点：引用一个 image (source) 与可选 sampler
struct GLTFTexture
{
    std::optional<std::size_t> image;    // 指向 images 数组中的 index (fastgltf::Texture::imageIndex)
    std::optional<std::size_t> sampler;  // 采样器索引（若需要后续使用）
    std::string name;                    // 纹理名称(如果 glTF 提供)
};
