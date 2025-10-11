#include <iostream>
#include <filesystem>
#include <string>

#include "gltf/import/GLTFImporter.h"
#include "gltf/GLTFModel.h"
#include "pure/Model.h"

// TexConv detection
namespace texconv { bool Initialize(std::filesystem::path *outPath=nullptr); }

namespace gltf
{
    pure::Model ConvertFromGLTF(const GLTFModel &src);
}

namespace exporters
{
    bool ExportPureModel(pure::Model &sm,const std::filesystem::path &outDir,bool exportImagesFlag,bool imagesOnly);
}

static void PrintUsage()
{
    std::cout << "Usage: gltf_exporter [--no-images | --images-only] <input.gltf|.glb> [output_dir]\n";
}

int main(int argc,char *argv[])
{
    // Detect TexConv (optional)
    std::filesystem::path texconvPath;
    if(texconv::Initialize(&texconvPath))
        std::cout << "[Init] TexConv available: " << texconvPath.string() << "\n";
    else
        std::cout << "[Init] TexConv not found (continuing without conversion)\n";

    if(argc<2)
    {
        PrintUsage();
        return 1;
    }

    bool exportImagesFlag=true;
    bool imagesOnly=false;

    int argIndex=1;
    for(;argIndex<argc; ++argIndex)
    {
        std::string a=argv[argIndex];
        if(a=="--no-images") { exportImagesFlag=false; continue; }
        if(a=="--images-only") { imagesOnly=true; continue; }
        // first non-flag assumed input path
        break;
    }

    if(argIndex>=argc)
    {
        PrintUsage();
        return 1;
    }

    std::filesystem::path inputPath=argv[argIndex++];
    std::filesystem::path outDir;
    if(argIndex<argc)
        outDir=argv[argIndex];

    if(imagesOnly && !exportImagesFlag)
    {
        std::cout << "[Warn] --images-only implies exporting images; ignoring --no-images.\n";
        exportImagesFlag=true;
    }

    GLTFModel model;
    if(!gltf::ImportFastGLTF(inputPath,model))
    {
        return 1;
    }
    
    pure::Model sm=gltf::ConvertFromGLTF(model);

    if(!exporters::ExportPureModel(sm,outDir,exportImagesFlag,imagesOnly))
    {
        return 1;
    }

    std::cout<<"[Export] Done: "<<inputPath<<"\n";
    return 0;
}
