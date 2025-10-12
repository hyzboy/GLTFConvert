#include "MeshExporter.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <filesystem>

#include <glm/glm.hpp>
#include <optional>

#include "math/BoundingVolumes.h"
#include "export/BoundingVolumesJson.h"

#include "pure/Model.h"
#include "pure/Primitive.h" // Primitive definition
#include "pure/Material.h"
#include "ExportFileNames.h"

namespace exporters
{

namespace
{
    // Compute bounding volumes for a mesh by aggregating positions from all
    // referenced primitives' geometries. Returns std::nullopt if no bounds can
    // be computed (not enough data).
    std::optional<BoundingVolumes> ComputeBoundsFromPrimitives(const pure::Model &model, const pure::Mesh &m)
    {
        if (m.primitives.size() <= 1)
            return std::nullopt;

        std::vector<glm::vec3> pts;
        pts.reserve(256);
        for (int32_t primIndex : m.primitives)
        {
            if (primIndex < 0 || primIndex >= static_cast<int32_t>(model.primitives.size()))
                continue;
            const auto &prim = model.primitives[primIndex];
            if (prim.geometry < 0 || prim.geometry >= static_cast<int32_t>(model.geometry.size()))
                continue;
            const auto &geom = model.geometry[prim.geometry];
            if (!geom.positions.has_value()) continue;
            const auto &pos = geom.positions.value();
            pts.insert(pts.end(), pos.begin(), pos.end());
        }

        if (pts.empty()) return std::nullopt;

        BoundingVolumes bv;
        bv.fromPoints(pts);

        if (bv.emptyAABB() && bv.emptyOBB() && bv.emptySphere())
            return std::nullopt;

        return bv;
    }

} // anonymous namespace
    // Export a single mesh at index `mi`. Returns false on error.
    bool ExportSingleMesh(pure::Model &model, std::size_t mi, const std::string &baseName, const std::filesystem::path &dir)
    {
        if (mi >= model.meshes.size()) return false;
        const auto &m = model.meshes[mi];

        nlohmann::json j;

        if (!m.name.empty()) j["name"] = m.name;

        nlohmann::json prims = nlohmann::json::array();

        for (int32_t primIndex : m.primitives)
        {
            if (primIndex < 0 || primIndex >= static_cast<int32_t>(model.primitives.size()))
                continue;

            const auto &prim = model.primitives[primIndex];

            nlohmann::json jp;

            if (prim.geometry >= 0 && prim.geometry < static_cast<int32_t>(model.geometry.size()))
            {
                jp["geometry"] = MakeGeometryFileName(baseName, prim.geometry);
            }
            if (prim.material.has_value())
            {
                int32_t matIdx = prim.material.value();
                if (matIdx >= 0 && matIdx < static_cast<int32_t>(model.materials.size()))
                {
                    const auto &mat = model.materials[matIdx];
                    jp["material"] = MakeMaterialFileName(baseName, mat->name, matIdx);
                }
            }
            prims.push_back(std::move(jp));
        }

        if (!prims.empty()) j["primitives"] = std::move(prims);

        if (auto maybeBV = ComputeBoundsFromPrimitives(model, m))
        {
            // write back into model's mesh so subsequent code can use it
            model.meshes[mi].bounding_volume = *maybeBV;
            j["bounds"] = BoundingVolumesToJson(*maybeBV);
        }

        auto filePath = dir / MakeMeshFileName(baseName, m.name, static_cast<int32_t>(mi));
        std::ofstream ofs(filePath, std::ios::binary);
        if (!ofs)
        {
            std::cerr << "[Export] Cannot open mesh json for write: " << filePath << "\n";
            return false;
        }
        ofs << j.dump(4);
        return true;
    }

    bool ExportMeshes(pure::Model &model, const std::filesystem::path &dir)
    {
        if (model.meshes.empty()) return true; // nothing to do

        std::error_code ec; std::filesystem::create_directories(dir, ec);
        std::string baseName = model.GetBaseName();

        for (std::size_t mi = 0; mi < model.meshes.size(); ++mi)
        {
            if (!ExportSingleMesh(model, mi, baseName, dir))
                return false;
        }
        return true;
    }
}
