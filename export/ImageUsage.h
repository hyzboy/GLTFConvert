#pragma once
#include <vector>
#include <cstddef>
#include <string>

namespace pure { struct Model; }

namespace exporters
{
    enum class ImageUsage
    {
        BaseColor,
        MetallicRoughness,
        Normal,
        Occlusion,
        Emissive,
        Specular,
        SpecularColor,
        Clearcoat,
        ClearcoatRoughness,
        ClearcoatNormal,
        SheenColor,
        SheenRoughness,
        Transmission,
        DiffuseTransmission,
        Thickness,
        Anisotropy,
        Iridescence,
        IridescenceThickness,
        Unknown
    };

    // 为所有 images 推理主要用途（size 与 model.images.size 一致）
    void BuildImageUsage(const pure::Model &model, std::vector<ImageUsage> &outUsages);

    // 便捷：单独推理一个图像（内部会遍历构建一次缓存，若需要大量调用请直接用 BuildImageUsage）
    ImageUsage GuessImageUsage(const pure::Model &model, std::size_t imageIndex);

    const char *ImageUsageToString(ImageUsage u);
}
