#include"GLTFExporter.h"
#include<iostream>

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cout << "Usage: gltf_exporter <input.gltf or .glb> [output_dir]\n";
        return 1;
    }
    std::filesystem::path inputPath = argv[1];
    std::filesystem::path outDir;
    if (argc >= 3) {
        outDir = argv[2];
    }
    if (!gltf::ExportToTOML(inputPath, outDir)) {
        return 1;
    }
    return 0;
}
