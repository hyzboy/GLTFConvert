#include <iostream>
#include <filesystem>

#include "gltf/import/GLTFImporter.h"
#include "gltf/GLTFModel.h"
#include "pure/Model.h"

namespace pure
{
    pure::Model ConvertFromGLTF(const GLTFModel &src);
}

namespace exporters
{
    bool ExportPureModel(pure::Model &sm,const std::filesystem::path &outDir);
}

int main(int argc,char *argv[])
{
    if(argc<2)
    {
        std::cout<<"Usage: gltf_exporter <input.gltf or .glb> [output_dir]\n";
        return 1;
    }
    std::filesystem::path inputPath=argv[1];
    std::filesystem::path outDir;
    if(argc>=3)
    {
        outDir=argv[2];
    }

    GLTFModel model;
    if(!gltf::ImportFastGLTF(inputPath,model))
    {
        return 1;
    }
    
    pure::Model sm=pure::ConvertFromGLTF(model);

    if(!exporters::ExportPureModel(sm,outDir))
    {
        return 1;
    }

    std::cout<<"[Export] Done: "<<inputPath<<"\n";
    return 0;
}
