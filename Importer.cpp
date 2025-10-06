#include "Importer.h"

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <iostream>

#include "import/ImportMaterials.h"
#include "import/ImportPrimitives.h"
#include "import/ImportMeshes.h"
#include "import/ImportNodes.h"
#include "import/ImportScenes.h"
#include "import/Orientation.h"

namespace importers
{
    bool ImportFastGLTF(const std::filesystem::path& inputPath, GLTFModel& outModel)
    {
        fastgltf::Parser parser{};
        auto dataRes = fastgltf::GltfDataBuffer::FromPath(inputPath);
        if (dataRes.error() != fastgltf::Error::None)
        {
            std::cerr << "[Import] Read failed: " << fastgltf::getErrorMessage(dataRes.error()) << "\n";
            return false;
        }
        auto data = std::move(dataRes.get());

        constexpr fastgltf::Options options = fastgltf::Options::LoadExternalBuffers
                                            | fastgltf::Options::LoadGLBBuffers
                                            | fastgltf::Options::DecomposeNodeMatrices
                                            | fastgltf::Options::GenerateMeshIndices;

        auto parent   = inputPath.parent_path();
        auto assetRes = parser.loadGltf(data, parent, options);
        if (assetRes.error() != fastgltf::Error::None)
        {
            std::cerr << "[Import] Parse failed: " << fastgltf::getErrorMessage(assetRes.error()) << "\n";
            return false;
        }
        fastgltf::Asset asset = std::move(assetRes.get());

        outModel.source = std::filesystem::absolute(inputPath).string();

        ImportMaterials(asset, outModel.materials);
        outModel.primitives.clear();
        outModel.primitives.reserve([&]{ std::size_t c=0; for(auto &m:asset.meshes) c+=m.primitives.size(); return c; }());
        ImportPrimitives(asset, outModel.primitives);
        ImportMeshes(asset, outModel.meshes);
        ImportNodes(asset, outModel.nodes);
        ImportScenes(asset, outModel.scenes);

        RotateNodeLocalTransformsYUpToZUp(outModel.nodes);
        RotatePrimitivesYUpToZUp(outModel.primitives);
        return true;
    }
} // namespace importers
