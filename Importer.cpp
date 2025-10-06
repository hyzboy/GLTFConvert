#include "Importer.h"

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <iostream>
#include <vector>

#include "gltf/GLTFMaterial.h"
#include "gltf/GLTFPrimitive.h"
#include "gltf/GLTFMesh.h"
#include "gltf/GLTFNode.h"
#include "gltf/GLTFScene.h"

namespace importers
{
    // Forward declarations (headers removed)
    void ImportMaterials(const fastgltf::Asset &asset,std::vector<GLTFMaterial> &materials);
    void ImportPrimitives(const fastgltf::Asset &asset,std::vector<GLTFPrimitive> &primitives);
    void ImportMeshes(const fastgltf::Asset &asset,std::vector<GLTFMesh> &meshes);
    void ImportNodes(const fastgltf::Asset &asset,std::vector<GLTFNode> &nodes);
    void ImportScenes(const fastgltf::Asset &asset,std::vector<GLTFScene> &scenes);
    void RotatePrimitivesYUpToZUp(std::vector<GLTFPrimitive> &primitives);
    void RotateNodeLocalTransformsYUpToZUp(std::vector<GLTFNode> &nodes);

    bool ImportFastGLTF(const std::filesystem::path &inputPath,GLTFModel &outModel)
    {
        fastgltf::Parser parser{};
        auto dataRes=fastgltf::GltfDataBuffer::FromPath(inputPath);

        if(dataRes.error()!=fastgltf::Error::None)
        {
            std::cerr<<"[Import] Read failed: "<<fastgltf::getErrorMessage(dataRes.error())<<"\n";
            return false;
        }

        auto data=std::move(dataRes.get());

        constexpr fastgltf::Options options=fastgltf::Options::LoadExternalBuffers
            |fastgltf::Options::LoadGLBBuffers
            |fastgltf::Options::DecomposeNodeMatrices
            |fastgltf::Options::GenerateMeshIndices;

        auto parent=inputPath.parent_path();
        auto assetRes=parser.loadGltf(data,parent,options);

        if(assetRes.error()!=fastgltf::Error::None)
        {
            std::cerr<<"[Import] Parse failed: "<<fastgltf::getErrorMessage(assetRes.error())<<"\n";
            return false;
        }

        fastgltf::Asset asset=std::move(assetRes.get());

        outModel.source=std::filesystem::absolute(inputPath).string();

        ImportMaterials(asset,outModel.materials);
        outModel.primitives.clear();
        outModel.primitives.reserve([&] { std::size_t c=0; for(auto &m:asset.meshes) c+=m.primitives.size(); return c; }());
        ImportPrimitives(asset,outModel.primitives);
        ImportMeshes(asset,outModel.meshes);
        ImportNodes(asset,outModel.nodes);
        ImportScenes(asset,outModel.scenes);

        RotateNodeLocalTransformsYUpToZUp(outModel.nodes);
        RotatePrimitivesYUpToZUp(outModel.primitives);
        return true;
    }
} // namespace importers
