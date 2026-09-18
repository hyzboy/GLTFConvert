#include "GLTFConvertCore.h"
#include <iostream>
#include <string>
#include <vector>

static void PrintUsage()
{
    std::cout << "Usage: GLTFConvert [options] <input.gltf|.glb> [output_dir]\n\n"
              << "Options:\n"
              << "  --no-images                 Skip exporting/converting textures\n"
              << "  --images-only               Only export textures, skip geometry/scenes\n"
              << "  --allow-u8-indices          Allow retaining 8-bit index buffers\n"
              << "  --normal-v2un8 (default)    Export normals as 2-component UNorm8 octahedral format\n"
              << "  --normal-v2hf               Export normals as 2-component half-float\n"
              << "  --normal-v3f                Export normals as 3-component float3\n"
              << "  --with-tangent              Compute and export vertex tangents\n"
              << "  --meshlet (default)         Generate Meshlets for GPU-driven rendering\n"
              << "  --no-meshlet                Disable generating Meshlets\n"
              << "  --config=<file.ini>         Specify GLTFConvert.ini configuration file path\n"
              << "  -h, --help                  Show this help message\n";
}

static void OnProgress(float percentage, const char *stepName, void *user_data)
{
    (void)user_data;
    int pct = static_cast<int>(percentage * 100.0f);
    std::cout << "[Progress " << pct << "%] " << (stepName ? stepName : "") << "\n";
}

int main(int argc, char *argv[])
{
    bool texconvAvailable = false;
    char texconvPath[1024] = {0};
    GLTFConvert_GetTexConvInfo(&texconvAvailable, texconvPath, sizeof(texconvPath));
    if (texconvAvailable)
        std::cout << "[Init] TexConv available: " << texconvPath << "\n";
    else
        std::cout << "[Init] TexConv not found (continuing without textures conversion)\n";

    if (argc < 2)
    {
        PrintUsage();
        return 1;
    }

    GLTFConvertOptions options;
    GLTFConvert_InitDefaultOptions(&options);
    options.progress_cb = OnProgress;

    std::string configFilePath;
    std::string inputPath;
    std::string outDir;

    int argIndex = 1;
    for (; argIndex < argc; ++argIndex)
    {
        std::string a = argv[argIndex];
        if (a == "-h" || a == "--help")
        {
            PrintUsage();
            return 0;
        }
        if (a == "--no-images") { options.export_images = false; continue; }
        if (a == "--images-only") { options.images_only = true; continue; }
        if (a == "--allow-u8-indices") { options.allow_u8_indices = true; continue; }
        if (a == "--normal-v2un8" || a == "--normal-format=v2un8") { options.normal_format = GLTF_NORMAL_V2UN8; continue; }
        if (a == "--normal-v2hf" || a == "--normal-format=v2hf") { options.normal_format = GLTF_NORMAL_V2HF; continue; }
        if (a == "--normal-v3f" || a == "--normal-format=v3f") { options.normal_format = GLTF_NORMAL_V3F; continue; }
        if (a == "--with-tangent") { options.export_tangent = true; continue; }
        if (a == "--meshlet" || a == "--enable-meshlet") { options.build_meshlets = true; continue; }
        if (a == "--no-meshlet" || a == "--disable-meshlet") { options.build_meshlets = false; continue; }
        if (a.rfind("--config=", 0) == 0)
        {
            configFilePath = a.substr(9);
            options.config_file = configFilePath.c_str();
            continue;
        }

        // First non-flag argument is input path
        break;
    }

    if (argIndex >= argc)
    {
        PrintUsage();
        return 1;
    }

    inputPath = argv[argIndex++];
    options.input_path = inputPath.c_str();

    if (argIndex < argc)
    {
        outDir = argv[argIndex++];
        options.output_dir = outDir.c_str();
    }

    char errBuf[1024] = {0};
    if (!GLTFConvert_Process(&options, errBuf, sizeof(errBuf)))
    {
        std::cerr << "[Error] Conversion failed: " << errBuf << "\n";
        return 1;
    }

    return 0;
}
