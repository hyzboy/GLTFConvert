#include <iostream>
#include <filesystem>

#include "Importer.h"
#include "Exporter.h"

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

    gltf::Model model;
    if (!importers::ImportFastGLTF(inputPath, model)) {
        return 1;
    }

    if (!exporters::ExportPureModel(model, outDir)) {
        return 1;
    }

    std::cout << "[Export] Done: " << inputPath << "\n";
    return 0;
}
