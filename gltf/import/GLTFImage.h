#pragma once
#include <string>
#include <vector>
#include <optional>
#include <cstddef>

// 简单封装 glTF image 数据（未做像素解码，只保留引用与原始字节/路径）
struct GLTFImage
{
    std::string name;            // image name
    std::string uri;             // 原始 uri (如果是外部文件或 data uri)
    std::string mimeType;        // fastgltf MimeType -> string
    std::vector<std::byte> data; // 原始字节，如果可用
    std::optional<std::size_t> bufferViewIndex; // 对应的 bufferView 索引
};
