#include "MaterialExporter.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <vector>
#include <filesystem>
#include <unordered_map>
#include <limits>

#include "pure/Material.h"
#include "pure/PBRMaterial.h"
#include "pure/Model.h"
#include "pure/Texture.h"
#include "pure/Image.h"
#include "pure/Sampler.h"
#include "ExportFileNames.h"
#include "ImageMime.h" // added
#include "SamplerStrings.h" // added

using nlohmann::json;

namespace exporters
{
    // Removed duplicated GuessImageExtension (now in ImageMime.cpp)

    // all material export logic for PBR is implemented in pure::PBRMaterial::toJson

    bool ExportMaterials(const pure::Model &model, const std::filesystem::path &dir)
    {
        if (model.materials.empty()) return true;

        std::error_code ec; std::filesystem::create_directories(dir, ec);
        std::string baseName = dir.filename().string();
        if (auto pos = baseName.find(".StaticMesh"); pos != std::string::npos) baseName = baseName.substr(0, pos);

        for (std::size_t i = 0; i < model.materials.size(); ++i)
        {
            const auto &matPtr = model.materials[i];
            const pure::Material *mat = matPtr.get();

            // Prepare empty remap maps by default. If the material is PBR we
            // fill them from its used* vectors so the material can produce
            // an export JSON with local indices.
            std::unordered_map<std::size_t,int32_t> texRemap, imgRemap, sampRemap;
            if (auto pm = dynamic_cast<const pure::PBRMaterial*>(mat))
            {
                for (std::size_t ti = 0; ti < pm->usedTextures.size(); ++ti) texRemap[pm->usedTextures[ti]] = static_cast<int32_t>(ti);
                for (std::size_t ii = 0; ii < pm->usedImages.size();   ++ii) imgRemap[pm->usedImages[ii]]   = static_cast<int32_t>(ii);
                for (std::size_t si = 0; si < pm->usedSamplers.size(); ++si) sampRemap[pm->usedSamplers[si]] = static_cast<int32_t>(si);
            }

            auto filePath = dir / MakeMaterialFileName(baseName, mat->name, static_cast<int32_t>(i));
            std::ofstream ofs(filePath, std::ios::binary);
            if (!ofs)
            {
                std::cerr << "[Export] Cannot write material: " << filePath << "\n";
                return false;
            }
            ofs << mat->toJson(model, texRemap, imgRemap, sampRemap, baseName).dump(4);
            ofs.close();
            std::cout << "[Export] Saved material: " << filePath << "\n";
        }
        return true;
    }
}
