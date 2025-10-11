#include "MeshExporter.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <filesystem>

#include "math/BoundingVolumes.h"
#include "export/BoundingVolumesJson.h"

#include "pure/Model.h"
#include "pure/Primitive.h" // Primitive definition
#include "pure/Material.h"
#include "ExportFileNames.h"

namespace exporters
{
    bool ExportMeshes(const pure::Model &model, const std::filesystem::path &dir)
    {
        if (model.meshes.empty()) return true; // nothing to do

        std::error_code ec; std::filesystem::create_directories(dir, ec);
        std::string baseName = model.GetBaseName();

        for (std::size_t mi = 0; mi < model.meshes.size(); ++mi)
        {
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

            // Use mesh-level bounding volumes computed during conversion, if present
            const auto &bv = m.bounding_volume;
            if (!bv.emptyAABB() || !bv.emptyOBB() || !bv.emptySphere())
            {
                j["bounds"] = BoundingVolumesToJson(bv);
            }

            auto filePath = dir / MakeMeshFileName(baseName, m.name, static_cast<int32_t>(mi));
            std::ofstream ofs(filePath, std::ios::binary);
            if (!ofs)
            {
                std::cerr << "[Export] Cannot open mesh json for write: " << filePath << "\n";
                return false;
            }
            ofs << j.dump(4);
        }
        return true;
    }
}
