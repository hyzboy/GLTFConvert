#include "SceneExportPack.h"
#include "SceneExportData.h"
#include "SceneExportNames.h"
#include "math/BoundingVolumes.h"

#include "mini_pack_builder.h" // use same builder API as geometry export
#include <iostream>
#include <vector>
#include <cstdint>

namespace exporters
{
    // Local helper mirroring ExportGeometry.cpp style
    static bool write_pack(MiniPackBuilder &builder, const std::filesystem::path &filePath, std::string &err)
    {
        auto writer = create_file_writer(filePath.string());
        if (!writer)
        {
            err = std::string("Cannot open file for writing: ") + filePath.string();
            return false;
        }
        MiniPackBuildResult result;
        if (!builder.build_pack(writer.get(), /*index_only*/false, result, err))
            return false;
        return true;
    }

    bool WriteScenePack(const SceneExportData &data, const std::filesystem::path &filePath)
    {
        MiniPackBuilder builder;
        std::string err;

        // Scene header (currently just sceneNameIndex, can extend later)
        if (!builder.add_entry_from_buffer("SceneHeader", &data.sceneNameIndex, static_cast<std::uint32_t>(sizeof(data.sceneNameIndex)), err))
        { std::cerr << "[Export] pack header fail: " << err << "\n"; return false; }

        // Name table
        if (!data.nameTable.empty())
        {
            write_string_list(&builder, "NameTable", data.nameTable, err);

            if (!err.empty()) { std::cerr << "[Export] pack name table fail: " << err << "\n"; return false; }
        }

        // Nodes stream (same int packing strategy as previous implementation)
        if (!data.nodes.empty())
        {
            std::vector<int32_t> nodeStream;
            size_t estimate = data.nodes.size() * 8;
            for (const auto &n : data.nodes) estimate += n.primitives.size() + n.children.size();
            nodeStream.reserve(estimate);
            for (const auto &n : data.nodes)
            {
                nodeStream.push_back(n.originalIndex);
                nodeStream.push_back(n.nameIndex);
                nodeStream.push_back(n.localMatrixIndex);
                nodeStream.push_back(n.worldMatrixIndex);
                nodeStream.push_back(n.trsIndex);
                nodeStream.push_back(n.boundsIndex);
                nodeStream.push_back(static_cast<int32_t>(n.primitives.size()));
                for (int32_t pm : n.primitives) nodeStream.push_back(pm);
                nodeStream.push_back(static_cast<int32_t>(n.children.size()));
                for (int32_t c : n.children) nodeStream.push_back(c);
            }
            if (!builder.add_entry_from_buffer("NodeList", nodeStream.data(), static_cast<std::uint32_t>(nodeStream.size() * sizeof(int32_t)), err))
            { std::cerr << "[Export] pack node list fail: " << err << "\n"; return false; }
        }

        // TRS table
        if (!data.trsTable.empty())
        {
            if (!builder.add_entry_from_buffer("TRSTable", data.trsTable.data(), static_cast<std::uint32_t>(data.trsTable.size() * sizeof(TRS)), err))
            { std::cerr << "[Export] pack TRS table fail: " << err << "\n"; return false; }
        }

        // Matrix table
        if (!data.matrixTable.empty())
        {
            if (!builder.add_entry_from_buffer("MatrixTable", data.matrixTable.data(), static_cast<std::uint32_t>(data.matrixTable.size() * sizeof(glm::mat4)), err))
            { std::cerr << "[Export] pack matrix table fail: " << err << "\n"; return false; }
        }

        // Bounds table
        if (!data.boundsTable.empty())
        {
            std::vector<PackedBounds> packed(data.boundsTable.size());
            for (size_t i = 0; i < data.boundsTable.size(); ++i)
                data.boundsTable[i].Pack(&packed[i]);
            if (!builder.add_entry_from_buffer("BoundsTable", packed.data(), static_cast<std::uint32_t>(packed.size() * sizeof(PackedBounds)), err))
            { std::cerr << "[Export] pack bounds table fail: " << err << "\n"; return false; }
        }

        // Primitive list (geometryFile length stored for now; filenames not inlined to keep format simple)
        if (!data.primitives.empty())
        {
            std::vector<int32_t> primStream;
            primStream.reserve(data.primitives.size() * 5);
            for (const auto &p : data.primitives)
            {
                primStream.push_back(p.originalIndex);
                primStream.push_back(p.geometryIndex);
                primStream.push_back(p.materialIndex.has_value() ? *p.materialIndex : -1);
                primStream.push_back(static_cast<int32_t>(p.geometryFile.size()));
            }
            if (!builder.add_entry_from_buffer("PrimitiveList", primStream.data(), static_cast<std::uint32_t>(primStream.size() * sizeof(int32_t)), err))
            { std::cerr << "[Export] pack primitive list fail: " << err << "\n"; return false; }
        }

        // Material list
        if (!data.materials.empty())
        {
            std::vector<int32_t> matStream;
            matStream.reserve(data.materials.size() * 2);
            for (const auto &m : data.materials)
            {
                matStream.push_back(m.originalIndex);
                matStream.push_back(static_cast<int32_t>(m.file.size()));
            }
            if (!builder.add_entry_from_buffer("MaterialList", matStream.data(), static_cast<std::uint32_t>(matStream.size() * sizeof(int32_t)), err))
            { std::cerr << "[Export] pack material list fail: " << err << "\n"; return false; }
        }

        // Geometry list
        if (!data.geometries.empty())
        {
            std::vector<int32_t> geoStream;
            geoStream.reserve(data.geometries.size() * 2);
            for (const auto &g : data.geometries)
            {
                geoStream.push_back(g.originalIndex);
                geoStream.push_back(static_cast<int32_t>(g.file.size()));
            }
            if (!builder.add_entry_from_buffer("GeometryList", geoStream.data(), static_cast<std::uint32_t>(geoStream.size() * sizeof(int32_t)), err))
            { std::cerr << "[Export] pack geometry list fail: " << err << "\n"; return false; }
        }

        if (!write_pack(builder, filePath, err))
        { std::cerr << "[Export] pack write fail: " << err << "\n"; return false; }
        return true;
    }
}
