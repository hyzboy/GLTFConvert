#include "SceneExportPack.h"
#include "SceneExportData.h"
#include "SceneExportNames.h"
#include "math/BoundingVolumes.h"

#include "mini_pack_builder.h" // use same builder API as geometry export
#include <iostream>
#include <vector>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <unordered_map>
#include <fstream>

namespace exporters
{
    namespace
    {
        constexpr uint32_t kScenePackV2Magic = 0x324E4353u; // 'SCN2'

        enum class SceneV2TableType : uint32_t
        {
            NameTable      = 1,
            NodeTable      = 2,
            NodePrimIndex  = 3,
            NodeChildIndex = 4,
            RootIndex      = 5,
            TRSTable       = 6,
            MatrixTable    = 7,
            BoundsTable    = 8,
            PrimitiveTable = 9,
            MaterialTable  = 10,
            GeometryTable  = 11,
            StringPool     = 12,
            GeometryViewTable = 13,
            GeometryBlob      = 14,
        };

#pragma pack(push, 1)
        struct ScenePackV2Header
        {
            uint32_t magic;
            uint16_t version_major;
            uint16_t version_minor;
            uint32_t flags;
            int32_t  scene_name_index;

            uint32_t node_count;
            uint32_t root_count;
            uint32_t primitive_count;
            uint32_t material_count;
            uint32_t geometry_count;

            uint32_t dir_offset;
            uint32_t dir_count;
            uint32_t payload_size;
            uint32_t reserved;
        };

        struct SceneV2TableDesc
        {
            uint32_t type;
            uint32_t offset;
            uint32_t size;
            uint32_t stride;
        };

        struct PackedNodeV2
        {
            int32_t original_index;
            int32_t name_index;
            int32_t local_matrix_index;
            int32_t world_matrix_index;
            int32_t trs_index;
            int32_t bounds_index;
            int32_t first_primitive;
            int32_t primitive_count;
            int32_t first_child;
            int32_t child_count;
        };

        struct PackedPrimitiveV2
        {
            int32_t  original_index;
            int32_t  geometry_index;
            int32_t  material_index;
            uint32_t geometry_file_offset;
            uint32_t geometry_file_length;
        };

        struct PackedMaterialV2
        {
            int32_t  original_index;
            uint32_t file_offset;
            uint32_t file_length;
        };

        struct PackedGeometryV2
        {
            int32_t  original_index;
            uint32_t file_offset;
            uint32_t file_length;
        };

        struct PackedGeometryViewV2
        {
            int32_t  original_index;
            uint32_t blob_offset;
            uint32_t blob_size;
            uint32_t blob_align;
        };

        // Flat TRS for binary I/O — 40 bytes, independent of GLM alignment defines.
        // glm::vec3 is 16 bytes under GLM_FORCE_DEFAULT_ALIGNED_GENTYPES; using float[]
        // ensures the on-disk layout always matches the loader's PackedTRS struct.
        struct PackedTRSFlat
        {
            float translation[3];  // 12 bytes
            float rotation[4];     // 16 bytes  (x, y, z, w)
            float scale[3];        // 12 bytes
        };
        static_assert(sizeof(PackedTRSFlat) == 40, "PackedTRSFlat must be 40 bytes");
#pragma pack(pop)

        static std::vector<uint8_t> BuildNameTableBytes(const std::vector<std::string> &name_table)
        {
            std::vector<uint8_t> out;

            const uint32_t count = static_cast<uint32_t>(name_table.size());
            out.resize(sizeof(uint32_t));
            std::memcpy(out.data(), &count, sizeof(uint32_t));

            out.reserve(sizeof(uint32_t) + count + count * 8);
            for (const auto &s : name_table)
                out.push_back(static_cast<uint8_t>(std::min<size_t>(255, s.size())));

            for (const auto &s : name_table)
            {
                const uint8_t len = static_cast<uint8_t>(std::min<size_t>(255, s.size()));
                out.insert(out.end(), s.data(), s.data() + len);
                out.push_back(0);
            }

            return out;
        }

        static std::vector<PackedTRSFlat> MakePackedTRS(const std::vector<TRS> &table)
        {
            std::vector<PackedTRSFlat> out;
            out.reserve(table.size());
            for (const auto &t : table)
            {
                PackedTRSFlat f{};
                f.translation[0] = t.translation.x;
                f.translation[1] = t.translation.y;
                f.translation[2] = t.translation.z;
                f.rotation[0]    = t.rotation.x;    // glm::quat memory order: x,y,z,w
                f.rotation[1]    = t.rotation.y;
                f.rotation[2]    = t.rotation.z;
                f.rotation[3]    = t.rotation.w;
                f.scale[0]       = t.scale.x;
                f.scale[1]       = t.scale.y;
                f.scale[2]       = t.scale.z;
                out.push_back(f);
            }
            return out;
        }

        static bool WriteScenePackV2Entries(MiniPackBuilder &builder, const SceneExportData &data, const std::filesystem::path &pack_dir, std::string &err)
        {
            std::cout << "[V2 Export] WriteScenePackV2Entries:"
                      << " nodes=" << data.nodes.size()
                      << " prims=" << data.primitives.size()
                      << " geos=" << data.geometries.size()
                      << " mats=" << data.materials.size() << "\n";

            std::vector<PackedNodeV2> nodes;
            std::vector<int32_t> node_prim_index;
            std::vector<int32_t> node_child_index;
            std::vector<int32_t> root_index = data.rootNodes;

            nodes.reserve(data.nodes.size());

            for (const auto &n : data.nodes)
            {
                PackedNodeV2 pn{};
                pn.original_index = n.originalIndex;
                pn.name_index = n.nameIndex;
                pn.local_matrix_index = n.localMatrixIndex;
                pn.world_matrix_index = n.worldMatrixIndex;
                pn.trs_index = n.trsIndex;
                pn.bounds_index = n.boundsIndex;

                pn.first_primitive = static_cast<int32_t>(node_prim_index.size());
                pn.primitive_count = static_cast<int32_t>(n.primitives.size());
                node_prim_index.insert(node_prim_index.end(), n.primitives.begin(), n.primitives.end());

                pn.first_child = static_cast<int32_t>(node_child_index.size());
                pn.child_count = static_cast<int32_t>(n.children.size());
                node_child_index.insert(node_child_index.end(), n.children.begin(), n.children.end());

                nodes.push_back(pn);
            }

            std::vector<uint8_t> string_pool;
            string_pool.reserve(2048);
            std::unordered_map<std::string, std::pair<uint32_t, uint32_t>> string_cache;

            auto add_string = [&](const std::string &s) -> std::pair<uint32_t, uint32_t>
            {
                const auto it = string_cache.find(s);
                if (it != string_cache.end())
                    return it->second;

                const uint32_t off = static_cast<uint32_t>(string_pool.size());
                const uint32_t len = static_cast<uint32_t>(s.size());
                string_pool.insert(string_pool.end(), s.begin(), s.end());
                string_cache.emplace(s, std::make_pair(off, len));
                return { off, len };
            };

            std::vector<PackedPrimitiveV2> primitives;
            primitives.reserve(data.primitives.size());
            for (const auto &p : data.primitives)
            {
                const auto [off, len] = add_string(p.geometryFile);
                PackedPrimitiveV2 pp{};
                pp.original_index = p.originalIndex;
                pp.geometry_index = p.geometryIndex;
                pp.material_index = p.materialIndex.has_value() ? *p.materialIndex : -1;
                pp.geometry_file_offset = off;
                pp.geometry_file_length = len;
                primitives.push_back(pp);
            }

            std::cout << "[V2 Export] PackedPrimitiveV2 count=" << primitives.size() << "\n";
            for (std::size_t _pi = 0, _plim = std::min<std::size_t>(primitives.size(), 8); _pi < _plim; ++_pi)
                std::cout << "[V2 Export]  prim[" << _pi << "] orig=" << primitives[_pi].original_index
                          << " geo=" << primitives[_pi].geometry_index
                          << " mat=" << primitives[_pi].material_index
                          << " foff=" << primitives[_pi].geometry_file_offset
                          << " flen=" << primitives[_pi].geometry_file_length << "\n";

            std::vector<PackedMaterialV2> materials;
            materials.reserve(data.materials.size());
            for (const auto &m : data.materials)
            {
                const auto [off, len] = add_string(m.file);
                PackedMaterialV2 pm{};
                pm.original_index = m.originalIndex;
                pm.file_offset = off;
                pm.file_length = len;
                materials.push_back(pm);
            }

            std::vector<PackedGeometryV2> geometries;
            geometries.reserve(data.geometries.size());
            for (const auto &g : data.geometries)
            {
                const auto [off, len] = add_string(g.file);
                PackedGeometryV2 pg{};
                pg.original_index = g.originalIndex;
                pg.file_offset = off;
                pg.file_length = len;
                geometries.push_back(pg);
            }

            ByteStreamBuffer geometry_blob;
            std::vector<PackedGeometryViewV2> geometry_views;
            geometry_views.reserve(data.geometries.size());

            for (const auto &g : data.geometries)
            {
                const std::filesystem::path geo_path = pack_dir / g.file;
                std::ifstream ifs(geo_path, std::ios::binary | std::ios::ate);
                if (!ifs)
                {
                    err = std::string("Cannot open geometry file for ScenePayloadV2: ") + geo_path.string();
                    return false;
                }

                const std::streamsize sz = ifs.tellg();
                if (sz < 0)
                {
                    err = std::string("Cannot query geometry file size for ScenePayloadV2: ") + geo_path.string();
                    return false;
                }

                ifs.seekg(0, std::ios::beg);

                std::vector<uint8_t> geo_bytes(static_cast<size_t>(sz));
                if (sz > 0 && !ifs.read(reinterpret_cast<char *>(geo_bytes.data()), sz))
                {
                    err = std::string("Cannot read geometry file for ScenePayloadV2: ") + geo_path.string();
                    return false;
                }

                const uint32_t blob_off = geometry_blob.append(
                    geo_bytes.empty() ? nullptr : geo_bytes.data(),
                    static_cast<uint32_t>(geo_bytes.size()),
                    64);

                PackedGeometryViewV2 gv{};
                gv.original_index = g.originalIndex;
                gv.blob_offset = blob_off;
                gv.blob_size = static_cast<uint32_t>(geo_bytes.size());
                gv.blob_align = 64;
                geometry_views.push_back(gv);
            }

            std::cout << "[V2 Export] GeometryViewTable count=" << geometry_views.size()
                      << " blob_total=" << geometry_blob.buf.size() << "\n";
            for (std::size_t _gi = 0, _glim = std::min<std::size_t>(geometry_views.size(), 8); _gi < _glim; ++_gi)
                std::cout << "[V2 Export]  geoview[" << _gi << "] orig=" << geometry_views[_gi].original_index
                          << " off=" << geometry_views[_gi].blob_offset
                          << " sz=" << geometry_views[_gi].blob_size
                          << " file=" << data.geometries[_gi].file << "\n";

            std::vector<PackedBounds> packed_bounds;
            packed_bounds.resize(data.boundsTable.size());
            for (size_t i = 0; i < data.boundsTable.size(); ++i)
                data.boundsTable[i].Pack(&packed_bounds[i]);

            const std::vector<uint8_t> name_table_bytes = BuildNameTableBytes(data.nameTable);

            struct TableBlob
            {
                SceneV2TableType type;
                const void *data;
                uint32_t size;
                uint32_t stride;
                uint32_t align;
            };

            std::vector<TableBlob> blobs;
            blobs.reserve(12);

            auto push_blob = [&](SceneV2TableType t, const void *ptr, uint32_t size, uint32_t stride, uint32_t align)
            {
                if (size == 0)
                    return;
                blobs.push_back({ t, ptr, size, stride, align });
            };

            push_blob(SceneV2TableType::NameTable, name_table_bytes.data(), static_cast<uint32_t>(name_table_bytes.size()), 0, 16);
            push_blob(SceneV2TableType::NodeTable, nodes.data(), static_cast<uint32_t>(nodes.size() * sizeof(PackedNodeV2)), sizeof(PackedNodeV2), 16);
            push_blob(SceneV2TableType::NodePrimIndex, node_prim_index.data(), static_cast<uint32_t>(node_prim_index.size() * sizeof(int32_t)), sizeof(int32_t), 16);
            push_blob(SceneV2TableType::NodeChildIndex, node_child_index.data(), static_cast<uint32_t>(node_child_index.size() * sizeof(int32_t)), sizeof(int32_t), 16);
            push_blob(SceneV2TableType::RootIndex, root_index.data(), static_cast<uint32_t>(root_index.size() * sizeof(int32_t)), sizeof(int32_t), 16);
            const auto packed_trs_v2 = MakePackedTRS(data.trsTable);
            push_blob(SceneV2TableType::TRSTable, packed_trs_v2.data(), static_cast<uint32_t>(packed_trs_v2.size() * sizeof(PackedTRSFlat)), sizeof(PackedTRSFlat), 16);
            push_blob(SceneV2TableType::MatrixTable, data.matrixTable.data(), static_cast<uint32_t>(data.matrixTable.size() * sizeof(glm::mat4)), sizeof(glm::mat4), 16);
            push_blob(SceneV2TableType::BoundsTable, packed_bounds.data(), static_cast<uint32_t>(packed_bounds.size() * sizeof(PackedBounds)), sizeof(PackedBounds), 16);
            push_blob(SceneV2TableType::PrimitiveTable, primitives.data(), static_cast<uint32_t>(primitives.size() * sizeof(PackedPrimitiveV2)), sizeof(PackedPrimitiveV2), 16);
            push_blob(SceneV2TableType::MaterialTable, materials.data(), static_cast<uint32_t>(materials.size() * sizeof(PackedMaterialV2)), sizeof(PackedMaterialV2), 16);
            push_blob(SceneV2TableType::GeometryTable, geometries.data(), static_cast<uint32_t>(geometries.size() * sizeof(PackedGeometryV2)), sizeof(PackedGeometryV2), 16);
            push_blob(SceneV2TableType::StringPool, string_pool.data(), static_cast<uint32_t>(string_pool.size()), 1, 16);
            push_blob(SceneV2TableType::GeometryViewTable, geometry_views.data(), static_cast<uint32_t>(geometry_views.size() * sizeof(PackedGeometryViewV2)), sizeof(PackedGeometryViewV2), 16);
            push_blob(SceneV2TableType::GeometryBlob, geometry_blob.buf.data(), static_cast<uint32_t>(geometry_blob.buf.size()), 1, 64);

            std::cout << "[V2 Export] Blobs: " << blobs.size() << " tables\n";
            for (const auto &_b : blobs)
                std::cout << "[V2 Export]  type=" << static_cast<uint32_t>(_b.type) << " size=" << _b.size << "\n";

            ByteStreamBuffer pb;
            const uint32_t dir_offset = pb.append(nullptr, static_cast<uint32_t>(blobs.size() * sizeof(SceneV2TableDesc)), 4);

            std::vector<SceneV2TableDesc> descs;
            descs.reserve(blobs.size());
            for (const auto &b : blobs)
            {
                const uint32_t off = pb.append(b.data, b.size, b.align);
                SceneV2TableDesc d{};
                d.type = static_cast<uint32_t>(b.type);
                d.offset = off;
                d.size = b.size;
                d.stride = b.stride;
                descs.push_back(d);
            }

            std::memcpy(pb.buf.data() + dir_offset, descs.data(), descs.size() * sizeof(SceneV2TableDesc));

            ScenePackV2Header header{};
            header.magic = kScenePackV2Magic;
            header.version_major = 2;
            header.version_minor = 0;
            header.flags = 1; // little-endian
            header.scene_name_index = data.sceneNameIndex;
            header.node_count = static_cast<uint32_t>(data.nodes.size());
            header.root_count = static_cast<uint32_t>(data.rootNodes.size());
            header.primitive_count = static_cast<uint32_t>(data.primitives.size());
            header.material_count = static_cast<uint32_t>(data.materials.size());
            header.geometry_count = static_cast<uint32_t>(data.geometries.size());
            header.dir_offset = dir_offset;
            header.dir_count = static_cast<uint32_t>(descs.size());
            header.payload_size = static_cast<uint32_t>(pb.buf.size());

            std::cout << "[V2 Export] Header: payload_size=" << header.payload_size
                      << " dir_count=" << header.dir_count
                      << " primitive_count=" << header.primitive_count
                      << " geometry_count=" << header.geometry_count << "\n";

            if (!builder.add_entry_from_buffer("SceneHeaderV2", &header, static_cast<uint32_t>(sizeof(ScenePackV2Header)), err))
                return false;

            if (!builder.add_entry_from_buffer("ScenePayloadV2", pb.buf.data(), static_cast<uint32_t>(pb.buf.size()), err))
                return false;

            return true;
        }
    }

    // -------------------------------------------------------------------------
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
            const auto packed_trs_v1 = MakePackedTRS(data.trsTable);
            if (!builder.add_entry_from_buffer("TRSTable", packed_trs_v1.data(), static_cast<std::uint32_t>(packed_trs_v1.size() * sizeof(PackedTRSFlat)), err))
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

        // RootList: uint32 count + int32[count] of scene-local root node indices
        if (!data.rootNodes.empty())
        {
            std::vector<int32_t> rootStream;
            rootStream.reserve(1 + data.rootNodes.size());
            rootStream.push_back(static_cast<int32_t>(data.rootNodes.size()));
            for (int32_t r : data.rootNodes) rootStream.push_back(r);
            if (!builder.add_entry_from_buffer("RootList", rootStream.data(), static_cast<std::uint32_t>(rootStream.size() * sizeof(int32_t)), err))
            { std::cerr << "[Export] pack root list fail: " << err << "\n"; return false; }
        }

        // Primitive list: variable-length byte stream with inline geometry filenames
        // Per entry: int32 originalIndex, int32 geometryIndex, int32 materialIndex, uint32 fileLen, char[fileLen]
        if (!data.primitives.empty())
        {
            ByteStreamBuffer b;
            for (const auto &p : data.primitives)
            {
                b.push_i32(p.originalIndex);
                b.push_i32(p.geometryIndex);
                b.push_i32(p.materialIndex.has_value() ? *p.materialIndex : -1);
                b.push_str(p.geometryFile);
            }
            if (!builder.add_entry_from_buffer("PrimitiveList", b.buf, err))
            { std::cerr << "[Export] pack primitive list fail: " << err << "\n"; return false; }
        }

        // Material list: variable-length byte stream with inline filenames
        // Per entry: int32 originalIndex, uint32 fileLen, char[fileLen]
        if (!data.materials.empty())
        {
            ByteStreamBuffer b;
            for (const auto &m : data.materials)
            {
                b.push_i32(m.originalIndex);
                b.push_str(m.file);
            }
            if (!builder.add_entry_from_buffer("MaterialList", b.buf, err))
            { std::cerr << "[Export] pack material list fail: " << err << "\n"; return false; }
        }

        // Geometry list: variable-length byte stream with inline filenames
        // Per entry: int32 originalIndex, uint32 fileLen, char[fileLen]
        if (!data.geometries.empty())
        {
            ByteStreamBuffer b;
            for (const auto &g : data.geometries)
            {
                b.push_i32(g.originalIndex);
                b.push_str(g.file);
            }
            if (!builder.add_entry_from_buffer("GeometryList", b.buf, err))
            { std::cerr << "[Export] pack geometry list fail: " << err << "\n"; return false; }
        }

        // V2 block-based layout for low-I/O loading path.
        if (!WriteScenePackV2Entries(builder, data, filePath.parent_path(), err))
        { std::cerr << "[Export] pack v2 write fail: " << err << "\n"; return false; }

        if (!write_pack(builder, filePath, err))
        { std::cerr << "[Export] pack write fail: " << err << "\n"; return false; }
        return true;
    }
}
