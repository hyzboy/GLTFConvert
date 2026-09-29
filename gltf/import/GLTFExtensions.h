#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <fastgltf/core.hpp>

namespace gltf
{
    /// 转换器启用解析的扩展集合（fastgltf 在**构造期**接收扩展位掩码：
    /// `fastgltf::Parser(Extensions)`，没有 setter）。
    ///
    /// 只启用"能消费或可安全忽略、且不会让产出几何出错"的扩展；**几何解压类**
    /// （KHR_draco_mesh_compression / EXT_meshopt_compression）与**实例化 / 双精度 accessor /
    /// 已废弃材质模型**一律不启用 —— 参考 `CheckRequiredExtensions()` 的拒绝理由。
    fastgltf::Extensions EnabledExtensions();

    /// 读源文件声明的 `extensionsRequired`（`.gltf` 直接扫 JSON；`.glb` 先定位 JSON chunk）。
    /// 只做"取名字列表"这一件事，找不到就返回空。
    std::vector<std::string> ReadRequiredExtensions(const std::filesystem::path &path);

    /// 预筛结果
    struct ExtensionCheck
    {
        bool ok { true };
        std::string error;                  ///< ok=false 时的原因（含扩展名与处置建议）
        std::vector<std::string> rejected;  ///< 无法转换的必需扩展
        std::vector<std::string> unhandled; ///< 已启用解析、但转换器**未实现其效果**的必需扩展（需告警，避免静默降级）
    };

    /// 预筛源文件能不能转换，以及哪些效果会被丢掉。
    ///
    /// **为什么必须在解析前做**：fastgltf 的"必需扩展未启用"失败路径在本项目里不干净
    /// （实测：打了一行错误后进程挂住 >120s 或被 abort，rc=3，且不打印主程序的
    /// `[Error] Conversion failed:`）—— 所以先自己判定，避免走进那条路。
    ExtensionCheck CheckRequiredExtensions(const std::filesystem::path &path);
}
