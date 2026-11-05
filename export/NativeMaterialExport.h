#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include "ImageUsage.h"

namespace pure { struct Model; }

namespace exporters
{
    // 导出图片的最终文件名（不含 Tex2D 转换），与 ExportImages 写盘命名规则完全一致：
    //   有名字   -> <base>.<name><ext>
    //   无名字   -> <base>.image.<idx>[.<usage>]<ext>（usage 已知时注入）
    std::string MakeExportedImageFileName(const pure::Model &model,
                                          std::size_t imageIndex,
                                          const std::string &baseName,
                                          const std::vector<ImageUsage> &usages);

    // 由纹理下标解析对应导出图片的文件名主干（不含扩展名）。失败返回空串。
    std::string TextureImageStem(const pure::Model &model,
                                 std::size_t textureIndex,
                                 const std::string &baseName,
                                 const std::vector<ImageUsage> &usages);

    // 阶段四：原生材质导出（替代旧的 .material JSON 逐材质文件）。
    // 写出与引擎自包含装配（BuildSelfContainedSceneMaterials）直接对接的两类 TOML：
    //   1. GLTF.<base>.<MatName>.recipe.toml —— 每个命名 PBR 材质一份配方（domain=StandardOpaque，
    //      槽位 base_color/normal/orm/emissive，asset 指向 <图片名干>.Tex2D）；
    //   2. <base>.material_data.toml        —— 单文件 PBRSurface 数据块，行序 = glTF 材质下标。
    // .scene 的 MaterialTable（tag 9）继续以 .material 文件名字符串承载材质名（引擎名匹配来源），
    // 但对应的 JSON 文件不再落盘。
    bool ExportNativeMaterials(const pure::Model &model, const std::filesystem::path &targetDir);
}
