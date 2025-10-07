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
        uint32_t version { 1 };
        uint32_t nodeCount { 0 };
        uint32_t subMeshCount { 0 };
        uint32_t materialCount { 0 };
        uint32_t geometryCount { 0 };
    };

    bool WriteScenePack(const SceneExportData &data, const std::filesystem::path &filePath)
    {
        MiniPackBuilder builder;

        ScenePackHeader hdr;
        hdr.nodeCount     = static_cast<uint32_t>(data.nodes.size());
        hdr.subMeshCount  = static_cast<uint32_t>(data.subMeshes.size());
        hdr.materialCount = static_cast<uint32_t>(data.materials.size());
        hdr.geometryCount = static_cast<uint32_t>(data.geometries.size());

        std::string err;
        if (!builder.add_entry_from_buffer("ScenePackHeader", &hdr, sizeof(hdr), err))
        {
            std::cerr << "[Export] pack header fail: " << err << "\n";
            return false;
        }

        json mirror;
        if (!data.sceneName.empty())
            mirror["name"] = data.sceneName;

        // Nodes
        mirror["nodes"] = json::array();
        for (const auto &n : data.nodes)
        {
            json jn;
            jn["i"] = n.originalIndex;

            json mt = json::array();
            for (int c = 0; c < 4; ++c)
            {
                for (int r = 0; r < 4; ++r)
                {
                    mt.push_back(n.localMatrix[c][r]);
                }
            }
            jn["m"] = mt;

            if (!n.subMeshes.empty())
                jn["s"] = n.subMeshes;
            if (!n.children.empty())
                jn["c"] = n.children;
            if (!n.name.empty())
                jn["n"] = n.name;

            mirror["nodes"].push_back(std::move(jn));
        }

        // SubMeshes
        mirror["subMeshes"] = json::array();
        for (const auto &s : data.subMeshes)
        {
            json js;
            js["i"] = s.originalIndex;
            js["g"] = s.geometryIndex;
            if (s.materialIndex.has_value())
                js["m"] = s.materialIndex.value();
            if (!s.geometryFile.empty())
                js["f"] = s.geometryFile;
            mirror["subMeshes"].push_back(std::move(js));
        }

        // Materials
        if (!data.materials.empty())
        {
            mirror["materials"] = json::array();
            for (const auto &m : data.materials)
            {
                json jm;
                jm["i"] = m.originalIndex;
                if (!m.name.empty())
                    jm["n"] = m.name;
                mirror["materials"].push_back(std::move(jm));
            }
        }

        // Geometries
        if (!data.geometries.empty())
        {
            mirror["geometries"] = json::array();
            for (const auto &g : data.geometries)
            {
                json gj;
                gj["i"] = g.originalIndex;
                gj["f"] = g.file;
                mirror["geometries"].push_back(std::move(gj));
            }
        }

        std::string compact = mirror.dump();
        if (!builder.add_entry_from_buffer("SceneJson", compact.data(), static_cast<uint32_t>(compact.size()), err))
        {
            std::cerr << "[Export] pack json entry fail: " << err << "\n";
            return false;
        }

        auto writer = create_file_writer(filePath.string());
        if (!writer)
        {
            std::cerr << "[Export] Cannot open scene pack file: " << filePath << "\n";
            return false;
        }

        MiniPackBuildResult result;
        if (!builder.build_pack(writer.get(), false, result, err))
        {
            std::cerr << "[Export] build pack fail: " << err << "\n";
            return false;
        }

        std::cout << "[Export] Saved scene pack: " << filePath << "\n";
        return true;
    }
}
