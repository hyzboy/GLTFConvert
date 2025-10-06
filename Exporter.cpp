#include "Exporter.h"

#include <iostream>
#include <filesystem>
#include <cctype>

#include "pure/ModelCore.h"
#include "pure/MeshNode.h"
#include "pure/Scene.h"
#include "SceneExporter.h"
#include "MaterialExporter.h"

namespace exporters
{

    // ------------------------------------------------------------
    // Utility helpers
    // ------------------------------------------------------------
    static std::string stem_noext(const std::filesystem::path &p)
    {
        return p.stem().string();
    }

    static std::string SanitizeName(const std::string &in)
    {
        if(in.empty())
            return {};

        std::string out;
        out.reserve(in.size());
        for(char c:in)
        {
            if(std::isalnum(static_cast<unsigned char>(c))||c=='_'||c=='-')
                out.push_back(c);
            else
                out.push_back('_');
        }
        return out;
    }

    // ------------------------------------------------------------
    // Geometry export only
    // ------------------------------------------------------------
    static void ExportGeometries(
        const pure::Model &sm,
        const std::filesystem::path &targetDir
    )
    {
        for(std::size_t u=0; u<sm.geometry.size(); ++u)
        {
            const auto &g=sm.geometry[u];
            std::filesystem::path binName=stem_noext(sm.gltf_source)+"."+std::to_string(u)+".geometry";
            std::filesystem::path binPath=targetDir/binName;
            // geometry bounds are kept internal; we do not serialize any bounds file anymore
            const BoundingBox &geo_bounds=(g.boundsIndex!=pure::kInvalidBoundsIndex)
                ?sm.bounds[g.boundsIndex]
                :BoundingBox{};
            if(!pure::SaveGeometry(g,geo_bounds,binPath.string()))
                std::cerr<<"[Export] Failed to write geometry binary: "<<binPath<<"\n";
        }
    }

    // ------------------------------------------------------------
    // Per-scene export (only binary .scene file)
    // ------------------------------------------------------------
    static bool ExportScene(
        const pure::Model &sm,
        std::size_t                  si,
        const std::filesystem::path &targetDir,
        const std::string &baseName
    )
    {
        const auto &sc=sm.scenes[si];
        pure::SceneExporter sl=pure::SceneExporter::Build(sm,static_cast<int32_t>(si));

        std::string sceneFolderName;
        {
            std::string sane=SanitizeName(sc.name);
            if(sane.empty())
                sceneFolderName="Scene_"+std::to_string(si);
            else
                sceneFolderName=std::to_string(si)+"_"+sane;
        }

        std::filesystem::path sceneDir=targetDir/sceneFolderName;
        std::error_code ec;
        std::filesystem::create_directories(sceneDir,ec);

        // Only write the consolidated scene binary now
        if(!sl.WriteSceneBinary(sceneDir,baseName))
            return false;

        return true;
    }

    // ------------------------------------------------------------
    // Public entry
    // ------------------------------------------------------------
    bool ExportPureModel(
        const GLTFModel &model,
        const std::filesystem::path &outDir
    )
    {
        pure::Model sm=pure::ConvertFromGLTF(model);

        std::filesystem::path baseDir=outDir.empty()
            ?std::filesystem::path(sm.gltf_source).parent_path()
            :outDir;
        std::error_code ec;
        std::filesystem::create_directories(baseDir,ec);

        const std::string baseName=stem_noext(sm.gltf_source);
        std::filesystem::path targetDir=baseDir/(baseName+".StaticMesh");
        std::filesystem::create_directories(targetDir,ec);

        // Write geometry binaries
        ExportGeometries(sm,targetDir);

        // Separate materials export
        if(!ExportMaterials(sm.materials,targetDir))
            return false;

        // Scene binaries (one folder per scene)
        for(std::size_t si=0; si<sm.scenes.size(); ++si)
        {
            if(!ExportScene(sm,si,targetDir,baseName))
                return false;
        }

        return true;
    }

} // namespace exporters
