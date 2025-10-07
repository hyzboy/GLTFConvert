#include "SceneExportData.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include "mini_pack_builder.h"

namespace exporters
{
    using nlohmann::json;

    struct ScenePackHeader
    {
        uint32_t version { 1 }; // keep version 1
        uint32_t nodeCount { 0 };
        uint32_t subMeshCount { 0 };
        uint32_t materialCount { 0 };
        uint32_t geometryCount { 0 };
        uint32_t nameCount { 0 }; // number of names in table
    };

    bool WriteScenePack(const SceneExportData &data, const std::filesystem::path &filePath)
    {
        MiniPackBuilder builder;

        ScenePackHeader hdr;
        hdr.nodeCount     = static_cast<uint32_t>(data.nodes.size());
        hdr.subMeshCount  = static_cast<uint32_t>(data.subMeshes.size());
        hdr.materialCount = static_cast<uint32_t>(data.materials.size());
        hdr.geometryCount = static_cast<uint32_t>(data.geometries.size());
        hdr.nameCount     = static_cast<uint32_t>(data.nameTable.size());

        std::string err;
        if (!builder.add_entry_from_buffer("ScenePackHeader", &hdr, sizeof(hdr), err))
        {
            std::cerr << "[Export] pack header fail: " << err << "\n";
            return false;
        }

        // Name table (JSON array) - optional
        if (!data.nameTable.empty())
        {
            json nt = data.nameTable; // simple array of strings
            std::string dumped = nt.dump();
            if (!builder.add_entry_from_buffer("NameTable", dumped.data(), static_cast<uint32_t>(dumped.size()), err))
            {
                std::cerr << "[Export] pack name table fail: " << err << "\n";
                return false;
            }
        }

        // NOTE: Mirror (SceneMirror) intentionally removed per request; pack now only contains header + name table.
        // Future: if binary arrays for nodes / subMeshes / etc. are needed, add dedicated entries here.

        auto writer = create_file_writer(filePath.string());
        if (!writer)
        {
            std::cerr << "[Export] cannot open output pack file: " << filePath << "\n";
            return false;
        }
        MiniPackBuildResult result; // currently unused
        if (!builder.build_pack(writer.get(), false, result, err))
        {
            std::cerr << "[Export] build pack fail: " << err << "\n";
            return false;
        }
        return true;
    }
}
