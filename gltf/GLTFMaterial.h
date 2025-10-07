#pragma once

#include <string>
#include <optional>
#include <cstddef>
#include <limits>

// 通用纹理引用
struct GLTFTextureRef {
    std::optional<std::size_t> index; // 纹理索引
    int texCoord = 0;                 // 使用的 UV 通道
    float scale = 1.0f;               // normalScale / clearcoatNormal / anisotropy 等
    float strength = 1.0f;            // occlusionStrength 等
};

// 核心 PBR Metallic-Roughness 数据（非扩展，始终存在）
struct GLTFPBRMetallicRoughness {
    float baseColorFactor[4] {1.f,1.f,1.f,1.f};
    float metallicFactor = 1.f;
    float roughnessFactor = 1.f;
    GLTFTextureRef baseColorTexture;            // baseColorTexture
    GLTFTextureRef metallicRoughnessTexture;    // metallicRoughnessTexture
};

// KHR_materials_emissive_strength
struct GLTFExtEmissiveStrength { float emissiveStrength = 1.f; };
// KHR_materials_unlit (无数据, 仅存在性)
struct GLTFExtUnlit { };
// KHR_materials_ior
struct GLTFExtIOR { float ior = 1.5f; };
// KHR_materials_transmission
struct GLTFExtTransmission { float transmissionFactor = 0.f; GLTFTextureRef transmissionTexture; };
// KHR_materials_diffuse_transmission
struct GLTFExtDiffuseTransmission { float diffuseTransmissionFactor = 0.f; GLTFTextureRef diffuseTransmissionTexture; };
// KHR_materials_specular
struct GLTFExtSpecular {
    float specularFactor = 1.f;
    float specularColorFactor[3] {1.f,1.f,1.f};
    GLTFTextureRef specularTexture;
    GLTFTextureRef specularColorTexture;
};
// KHR_materials_clearcoat
struct GLTFExtClearcoat {
    float clearcoatFactor = 0.f;
    float clearcoatRoughnessFactor = 0.f;
    GLTFTextureRef clearcoatTexture;
    GLTFTextureRef clearcoatRoughnessTexture;
    GLTFTextureRef clearcoatNormalTexture; // scale 使用 scale 字段
};
// KHR_materials_sheen
struct GLTFExtSheen {
    float sheenColorFactor[3] {0.f,0.f,0.f};
    float sheenRoughnessFactor = 0.f;
    GLTFTextureRef sheenColorTexture;
    GLTFTextureRef sheenRoughnessTexture;
};
// KHR_materials_volume
struct GLTFExtVolume {
    float thicknessFactor = 0.f;
    GLTFTextureRef thicknessTexture;
    float attenuationColor[3] {1.f,1.f,1.f};
    float attenuationDistance = std::numeric_limits<float>::infinity();
};
// KHR_materials_anisotropy
struct GLTFExtAnisotropy {
    float strength = 0.f; // anisotropyStrength
    float rotation = 0.f; // anisotropyRotation
    GLTFTextureRef texture; // direction packed in texture
};
// KHR_materials_iridescence
struct GLTFExtIridescence {
    float factor = 0.f;
    float ior = 1.3f;
    float thicknessMinimum = 100.f;
    float thicknessMaximum = 400.f;
    GLTFTextureRef texture;          // iridescenceTexture
    GLTFTextureRef thicknessTexture; // iridescenceThicknessTexture
};

struct GLTFMaterial {
    std::string name;

    // 核心 PBR 与核心属性
    GLTFPBRMetallicRoughness pbr;
    GLTFTextureRef normalTexture;      // uses scale
    GLTFTextureRef occlusionTexture;   // uses strength
    GLTFTextureRef emissiveTexture;    // core
    float emissiveFactor[3] {0.f,0.f,0.f}; // core

    enum class AlphaMode { Opaque, Mask, Blend };
    AlphaMode alphaMode = AlphaMode::Opaque;
    float alphaCutoff = 0.5f; // for Mask
    bool doubleSided = false;

    // 扩展（通过 optional 判断是否存在）
    std::optional<GLTFExtEmissiveStrength> extEmissiveStrength;
    std::optional<GLTFExtUnlit> extUnlit;
    std::optional<GLTFExtIOR> extIOR;
    std::optional<GLTFExtTransmission> extTransmission;
    std::optional<GLTFExtDiffuseTransmission> extDiffuseTransmission;
    std::optional<GLTFExtSpecular> extSpecular;
    std::optional<GLTFExtClearcoat> extClearcoat;
    std::optional<GLTFExtSheen> extSheen;
    std::optional<GLTFExtVolume> extVolume;
    std::optional<GLTFExtAnisotropy> extAnisotropy;
    std::optional<GLTFExtIridescence> extIridescence;
};
