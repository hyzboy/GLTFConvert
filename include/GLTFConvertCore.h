#pragma once

#if defined(_WIN32)
    #if defined(GLTFCONVERT_EXPORTS)
        #define GLTFCONVERT_API __declspec(dllexport)
    #else
        #define GLTFCONVERT_API __declspec(dllimport)
    #endif
#else
    #define GLTFCONVERT_API __attribute__((visibility("default")))
#endif

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum GLTFLogLevel
{
    GLTF_LOG_VERBOSE = 0,
    GLTF_LOG_DEBUG   = 1,
    GLTF_LOG_INFO    = 2,
    GLTF_LOG_WARN    = 3,
    GLTF_LOG_ERROR   = 4
} GLTFLogLevel;

typedef enum GLTFNormalFormat
{
    GLTF_NORMAL_V2UN8 = 0, // 2分量 UNorm8 (默认)
    GLTF_NORMAL_V2HF  = 1, // 2分量 Half float
    GLTF_NORMAL_V3F   = 2  // 3分量 Float
} GLTFNormalFormat;

typedef void (*GLTFLogCallback)(GLTFLogLevel level, const char *msg, void *user_data);
typedef void (*GLTFProgressCallback)(float percentage, const char *step_name, void *user_data);

typedef struct GLTFConvertOptions
{
    const char *input_path;          // 必填：输入文件路径 (.gltf/.glb)
    const char *output_dir;          // 可选：输出目录，NULL 或 "" 表示与输入同目录
    const char *config_file;         // 可选：指定的 .ini 配置文件路径，NULL 则自动探测

    bool export_images;              // 是否导出并转换纹理 (默认 true)
    bool images_only;                // 是否仅导出纹理 (默认 false)
    bool allow_u8_indices;           // 是否允许 8 位索引 (默认 false)

    GLTFNormalFormat normal_format;  // 法线导出格式 (默认 GLTF_NORMAL_V2UN8)
    bool export_tangent;             // 是否导出切线 (默认 false)
    bool build_meshlets;             // 是否构建 Meshlets (默认 true)

    GLTFLogCallback      log_cb;     // 日志回调函数 (可选，为 NULL 时输出到标准控制台)
    GLTFProgressCallback progress_cb;// 进度回调函数 (可选，为 NULL 时忽略)
    void                *user_data;  // 用户自定义上下文指针，会传递给回调
} GLTFConvertOptions;

// 初始化为默认配置
GLTFCONVERT_API void GLTFConvert_InitDefaultOptions(GLTFConvertOptions *options);

// 检测并获取 TexConv 状态与路径
GLTFCONVERT_API bool GLTFConvert_GetTexConvInfo(bool *out_available, char *out_path, size_t max_path_len);

// 执行模型转换流程
GLTFCONVERT_API bool GLTFConvert_Process(const GLTFConvertOptions *options, char *err_buf, size_t err_buf_len);

#ifdef __cplusplus
}
#endif
