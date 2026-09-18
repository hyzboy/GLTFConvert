#include "GLTFConvertCore.h"

#include <iostream>
#include <filesystem>
#include <string>
#include <cstring>
#include <sstream>

#include "gltf/import/GLTFImporter.h"
#include "gltf/GLTFModel.h"
#include "pure/Model.h"
#include "TexConv.h"

namespace gltf
{
    pure::Model ConvertFromGLTF(const GLTFModel &src);
}

namespace exporters
{
    bool ExportPureModel(pure::Model &sm, const std::filesystem::path &outDir, bool exportImagesFlag, bool imagesOnly);
}

static void EmitLog(const GLTFConvertOptions *opt, GLTFLogLevel level, const std::string &msg)
{
    if (opt && opt->log_cb)
    {
        opt->log_cb(level, msg.c_str(), opt->user_data);
    }
    else
    {
        if (level == GLTF_LOG_ERROR)
            std::cerr << "[GLTFConvert Error] " << msg << "\n";
        else if (level == GLTF_LOG_WARN)
            std::cout << "[GLTFConvert Warn] " << msg << "\n";
        else
            std::cout << "[GLTFConvert] " << msg << "\n";
    }
}

static void EmitProgress(const GLTFConvertOptions *opt, float percentage, const char *stepName)
{
    if (opt && opt->progress_cb)
    {
        opt->progress_cb(percentage, stepName, opt->user_data);
    }
}

static void SetError(char *err_buf, size_t err_buf_len, const std::string &err)
{
    if (err_buf && err_buf_len > 0)
    {
        strncpy(err_buf, err.c_str(), err_buf_len - 1);
        err_buf[err_buf_len - 1] = '\0';
    }
}

extern "C"
{

void GLTFConvert_InitDefaultOptions(GLTFConvertOptions *options)
{
    if (!options) return;
    std::memset(options, 0, sizeof(*options));
    options->export_images    = true;
    options->images_only      = false;
    options->allow_u8_indices = false;
    options->normal_format    = GLTF_NORMAL_V2UN8;
    options->export_tangent   = false;
    options->build_meshlets   = true;
}

bool GLTFConvert_GetTexConvInfo(bool *out_available, char *out_path, size_t max_path_len)
{
    std::filesystem::path texPath;
    bool avail = texconv::Initialize(&texPath);
    if (out_available)
        *out_available = avail;

    if (out_path && max_path_len > 0)
    {
        std::string s = texPath.string();
        strncpy(out_path, s.c_str(), max_path_len - 1);
        out_path[max_path_len - 1] = '\0';
    }
    return avail;
}

bool GLTFConvert_Process(const GLTFConvertOptions *options, char *err_buf, size_t err_buf_len)
{
    if (err_buf && err_buf_len > 0)
        err_buf[0] = '\0';

    if (!options)
    {
        SetError(err_buf, err_buf_len, "options pointer is null");
        return false;
    }

    if (!options->input_path || options->input_path[0] == '\0')
    {
        std::string err = "Input file path is empty";
        EmitLog(options, GLTF_LOG_ERROR, err);
        SetError(err_buf, err_buf_len, err);
        return false;
    }

    std::filesystem::path inputPath(options->input_path);
    if (!std::filesystem::exists(inputPath))
    {
        std::string err = "Input file does not exist: " + inputPath.string();
        EmitLog(options, GLTF_LOG_ERROR, err);
        SetError(err_buf, err_buf_len, err);
        return false;
    }

    std::filesystem::path outDir;
    if (options->output_dir && options->output_dir[0] != '\0')
        outDir = options->output_dir;

    if (options->config_file && options->config_file[0] != '\0')
    {
        texconv::SetCustomConfigFile(options->config_file);
    }

    EmitProgress(options, 0.0f, "Init");

    std::filesystem::path texconvPath;
    if (texconv::Initialize(&texconvPath))
    {
        EmitLog(options, GLTF_LOG_INFO, "TexConv available: " + texconvPath.string());
    }
    else
    {
        EmitLog(options, GLTF_LOG_WARN, "TexConv not found (texture compression might be skipped or limited)");
    }

    bool exportImages = options->export_images;
    bool imagesOnly   = options->images_only;
    if (imagesOnly && !exportImages)
    {
        EmitLog(options, GLTF_LOG_WARN, "images_only is set, overriding export_images to true");
        exportImages = true;
    }

    gltf::SetAllowU8Indices(options->allow_u8_indices);
    gltf::SetNormalExportFormat(static_cast<pure::NormalExportFormat>(options->normal_format));
    gltf::SetExportTangent(options->export_tangent);
    gltf::SetBuildMeshlets(options->build_meshlets);

    EmitProgress(options, 0.15f, "Parsing glTF");
    EmitLog(options, GLTF_LOG_INFO, "Loading model: " + inputPath.string());

    GLTFModel model;
    if (!gltf::ImportFastGLTF(inputPath, model))
    {
        std::string err = "Failed to import glTF model from " + inputPath.string();
        EmitLog(options, GLTF_LOG_ERROR, err);
        SetError(err_buf, err_buf_len, err);
        return false;
    }

    EmitProgress(options, 0.45f, "Converting geometry");
    EmitLog(options, GLTF_LOG_INFO, "Converting glTF model to engine format...");
    pure::Model sm = gltf::ConvertFromGLTF(model);

    EmitProgress(options, 0.70f, "Exporting pure model");
    EmitLog(options, GLTF_LOG_INFO, "Exporting and packing assets...");
    if (!exporters::ExportPureModel(sm, outDir, exportImages, imagesOnly))
    {
        std::string err = "Failed to export pure model to destination directory";
        EmitLog(options, GLTF_LOG_ERROR, err);
        SetError(err_buf, err_buf_len, err);
        return false;
    }

    EmitProgress(options, 1.0f, "Done");
    EmitLog(options, GLTF_LOG_INFO, "Export completed successfully: " + inputPath.string());
    return true;
}

} // extern "C"
