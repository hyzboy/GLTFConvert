#include "SceneExportData.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

namespace exporters
{
    using nlohmann::json;

    bool WriteSceneJson(const SceneExportData &data, const std::filesystem::path &filePath)
    {
        json j;

        // name table
        if (!data.nameTable.empty())
            j["nameTable"] = data.nameTable;
        if (data.sceneNameIndex >= 0)
            j["nameIndex"] = data.sceneNameIndex;

        // Nodes
        j["nodes"] = json::array();
        for (const auto &n : data.nodes)
        {
            json jn;
            jn["index"] = n.originalIndex;
            if (n.nameIndex >= 0)
                jn["nameIndex"] = n.nameIndex;

            json arr = json::array();
            for (int c = 0; c < 4; ++c)
            {
                for (int r = 0; r < 4; ++r)
                {
                    arr.push_back(n.localMatrix[c][r]);
                }
            }
            jn["matrix"] = arr;

            if (!n.subMeshes.empty())
                jn["subMeshes"] = n.subMeshes;
            if (!n.children.empty())
                jn["children"] = n.children;

            j["nodes"].push_back(std::move(jn));
        }

        // SubMeshes
        j["subMeshes"] = json::array();
        for (const auto &s : data.subMeshes)
        {
            json js;
            js["index"] = s.originalIndex;
            js["geometry"] = s.geometryIndex;
            if (!s.geometryFile.empty())
                js["file"] = s.geometryFile;
            if (s.materialIndex.has_value())
                js["material"] = s.materialIndex.value();
            j["subMeshes"].push_back(std::move(js));
        }

        // Materials
        if (!data.materials.empty())
        {
            j["materials"] = json::array();
            for (const auto &m : data.materials)
            {
                json jm;
                jm["index"] = m.originalIndex;
                if (m.nameIndex >= 0)
                    jm["nameIndex"] = m.nameIndex;
                j["materials"].push_back(std::move(jm));
            }
        }

        // Geometries
        if (!data.geometries.empty())
        {
            j["geometries"] = json::array();
            for (const auto &g : data.geometries)
            {
                json gj;
                gj["index"] = g.originalIndex;
                gj["file"] = g.file;
                j["geometries"].push_back(std::move(gj));
            }
        }

        std::ofstream ofs(filePath, std::ios::binary);
        if (!ofs)
        {
            std::cerr << "[Export] Cannot open scene json file: " << filePath << "\n";
            return false;
        }

        ofs << j.dump(4);
        return true;
    }
}
