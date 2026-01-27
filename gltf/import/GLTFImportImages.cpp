#include <fastgltf/core.hpp>
#include <vector>
#include <string>
#include <cstring>
#include "gltf/GLTFImage.h"

namespace gltf
{
    static std::string MimeTypeToString(fastgltf::MimeType mt)
    {
        switch(mt)
        {
        case fastgltf::MimeType::JPEG: return "image/jpeg";
        case fastgltf::MimeType::PNG: return "image/png";
        case fastgltf::MimeType::KTX2: return "image/ktx2";
        case fastgltf::MimeType::DDS: return "image/vnd-ms.dds";
        case fastgltf::MimeType::WEBP: return "image/webp";
        default: return "";
        }
    }

    void ImportImages(const fastgltf::Asset &asset,std::vector<GLTFImage> &images)
    {
        images.clear();
        images.resize(asset.images.size());
        for(std::size_t i=0;i<asset.images.size();++i)
        {
            const auto &im=asset.images[i];
            auto &oi=images[i];
            if(!im.name.empty()) oi.name.assign(im.name.begin(),im.name.end());

            if(std::holds_alternative<fastgltf::sources::Vector>(im.data))
            {
                const auto &vec=std::get<fastgltf::sources::Vector>(im.data);
                oi.mimeType=MimeTypeToString(vec.mimeType);
                oi.data.resize(vec.bytes.size());
                std::memcpy(oi.data.data(),vec.bytes.data(),vec.bytes.size());
            }
            else if(std::holds_alternative<fastgltf::sources::ByteView>(im.data))
            {
                const auto &bv=std::get<fastgltf::sources::ByteView>(im.data);
                oi.mimeType=MimeTypeToString(bv.mimeType);
                oi.data.assign(bv.bytes.begin(),bv.bytes.end());
            }
            else if(std::holds_alternative<fastgltf::sources::Array>(im.data))
            {
                const auto &arr=std::get<fastgltf::sources::Array>(im.data);
                oi.mimeType=MimeTypeToString(arr.mimeType);
                oi.data.resize(arr.bytes.size());
                std::memcpy(oi.data.data(),arr.bytes.data(),arr.bytes.size());
            }
            else if(std::holds_alternative<fastgltf::sources::BufferView>(im.data))
            {
                const auto &bv=std::get<fastgltf::sources::BufferView>(im.data);
                oi.mimeType=MimeTypeToString(bv.mimeType);
                oi.bufferViewIndex=bv.bufferViewIndex;
            }
            else if(std::holds_alternative<fastgltf::sources::URI>(im.data))
            {
                // Could attempt to inspect mimeType; we leave uri empty to avoid unsupported reflection.
                const auto &u=std::get<fastgltf::sources::URI>(im.data);

                oi.mimeType=MimeTypeToString(u.mimeType);
                oi.uri=u.uri.string();
            }
        }
    }
} // namespace gltf
