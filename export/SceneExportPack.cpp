#include "SceneExportData.h"
#include <fstream>
#include <iostream>
#include "mini_pack_builder.h"

namespace exporters
{
    struct ScenePackHeader
    {
        uint32_t version { 1 }; // keep version 1
        uint32_t nodeCount { 0 };
        uint32_t subMeshCount { 0 };
        uint32_t materialCount { 0 };
        uint32_t geometryCount { 0 };
        uint32_t nameCount { 0 };      // number of names in table
        uint32_t trsCount { 0 };       // number of TRS entries
        uint32_t matrixCount { 0 };    // number of matrices
        uint32_t boundsCount { 0 };    // number of bounds entries
        int32_t  sceneNameIndex { -1 };   // index into name table
        int32_t  sceneBoundsIndex { -1 }; // index into bounds table
    };

    static std::string strip_extension(const std::string &fname)
    {
        auto pos = fname.find_last_of('.');
        if (pos == std::string::npos) return fname;
        return fname.substr(0, pos);
    }

    bool WriteScenePack(const SceneExportData &data, const std::filesystem::path &filePath)
    {
        MiniPackBuilder builder;

        ScenePackHeader hdr;
        hdr.nodeCount        = static_cast<uint32_t>(data.nodes.size());
        hdr.subMeshCount     = static_cast<uint32_t>(data.subMeshes.size());
        hdr.materialCount    = static_cast<uint32_t>(data.materials.size());
        hdr.geometryCount    = static_cast<uint32_t>(data.geometries.size());
        hdr.nameCount        = static_cast<uint32_t>(data.nameTable.size());
        hdr.trsCount         = static_cast<uint32_t>(data.trsTable.size());
        hdr.matrixCount      = static_cast<uint32_t>(data.matrixTable.size());
        hdr.boundsCount      = static_cast<uint32_t>(data.boundsTable.size());
        hdr.sceneNameIndex   = data.sceneNameIndex;
        hdr.sceneBoundsIndex = data.sceneBoundsIndex;

        std::string err;
        if (!builder.add_entry_from_buffer("SceneHeader", &hdr, sizeof(hdr), err))
        {
            std::cerr << "[Export] pack header fail: " << err << "\n";
            return false;
        }

        // Name table
        if (!data.nameTable.empty())
        {
            write_string_list(&builder, "NameTable", data.nameTable, err);
            if (!err.empty())
            {
                std::cerr << "[Export] pack name table fail: " << err << "\n";
                return false;
            }
        }

        // Node list
        if (!data.nodes.empty())
        {
            std::vector<int32_t> nodeStream;
            size_t estimate = data.nodes.size() * 8;
            for (const auto &n : data.nodes)
                estimate += n.subMeshes.size() + n.children.size();
            nodeStream.reserve(estimate);
            for (const auto &n : data.nodes)
            {
                nodeStream.push_back(n.originalIndex);
                nodeStream.push_back(n.nameIndex);
                nodeStream.push_back(n.localMatrixIndex);
                nodeStream.push_back(n.worldMatrixIndex);
                nodeStream.push_back(n.trsIndex);
                nodeStream.push_back(n.boundsIndex);
                nodeStream.push_back(static_cast<int32_t>(n.subMeshes.size()));
                for (int32_t sm : n.subMeshes) nodeStream.push_back(sm);
                nodeStream.push_back(static_cast<int32_t>(n.children.size()));
                for (int32_t c : n.children) nodeStream.push_back(c);
            }
            if (!builder.add_entry_from_buffer("NodeList", nodeStream.data(), static_cast<uint32_t>(nodeStream.size() * sizeof(int32_t)), err))
            {
                std::cerr << "[Export] pack node list fail: " << err << "\n";
                return false;
            }
        }

        // TRS table
        if (!data.trsTable.empty())
        {
            if (!builder.add_entry_from_buffer("TRSTable", data.trsTable.data(), static_cast<uint32_t>(data.trsTable.size() * sizeof(TRS)), err))
            {
                std::cerr << "[Export] pack TRS table fail: " << err << "\n";
                return false;
            }
        }

        // Matrix table
        if (!data.matrixTable.empty())
        {
            if (!builder.add_entry_from_buffer("MatrixTable", data.matrixTable.data(), static_cast<uint32_t>(data.matrixTable.size() * sizeof(glm::mat4)), err))
            {
                std::cerr << "[Export] pack matrix table fail: " << err << "\n";
                return false;
            }
        }

        // Bounds table
        if (!data.boundsTable.empty())
        {
            std::vector<PackedBounds> packed;
            packed.resize(data.boundsTable.size());
            for (size_t i = 0; i < data.boundsTable.size(); ++i)
                data.boundsTable[i].Pack(&packed[i]);
            if (!builder.add_entry_from_buffer("BoundsTable", packed.data(), static_cast<uint32_t>(packed.size() * sizeof(PackedBounds)), err))
            {
                std::cerr << "[Export] pack bounds table fail: " << err << "\n";
                return false;
            }
        }

        // Geometry list (strip extensions)
        if (!data.geometries.empty())
        {
            std::vector<std::string> geomFiles;
            geomFiles.reserve(data.geometries.size());
            for (const auto &g : data.geometries)
                geomFiles.push_back(strip_extension(g.file));
            write_string_list(&builder, "GeometryList", geomFiles, err);
            if (!err.empty())
            {
                std::cerr << "[Export] pack geometry list fail: " << err << "\n";
                return false;
            }
        }

        // Material list (strip extensions)
        if (!data.materials.empty())
        {
            std::vector<std::string> matFiles;
            matFiles.reserve(data.materials.size());
            for (const auto &m : data.materials)
                matFiles.push_back(strip_extension(m.file));
            write_string_list(&builder, "MaterialList", matFiles, err);
            if (!err.empty())
            {
                std::cerr << "[Export] pack material list fail: " << err << "\n";
                return false;
            }
        }

        // SubMesh list
        if (!data.subMeshes.empty())
        {
            std::vector<int32_t> pairs;
            pairs.reserve(data.subMeshes.size() * 2);
            for (const auto &s : data.subMeshes)
            {
                pairs.push_back(s.geometryIndex);
                pairs.push_back(s.materialIndex.has_value() ? s.materialIndex.value() : -1);
            }
            if (!builder.add_entry_from_buffer("SubMeshList", pairs.data(), static_cast<uint32_t>(pairs.size() * sizeof(int32_t)), err))
            {
                std::cerr << "[Export] pack submesh list fail: " << err << "\n";
                return false;
            }
        }

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
