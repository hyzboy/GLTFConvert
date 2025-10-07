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

    // 若已经把二进制加载到内存（external 或 GLB buffer），保存原始字节
    // 对于外部文件可能为空（可后续再读取），这里保留解析器提供的数据副本（若可取得）。
    std::vector<std::byte> data; // 原始字节，如果可用

    // 对应的 bufferView 索引（若通过 bufferView 提供）
    std::optional<std::size_t> bufferViewIndex;
};
