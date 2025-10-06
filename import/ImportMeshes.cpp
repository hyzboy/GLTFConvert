#include "ImportMeshes.h"

namespace importers {

void ImportMeshes(const fastgltf::Asset& asset, GLTFModel& outModel)
{
    std::size_t primCursor = 0;
    outModel.meshes.clear();
    outModel.meshes.reserve(asset.meshes.size());
    for (const auto& mesh : asset.meshes) {
        GLTFMesh m{};
        if (!mesh.name.empty()) m.name.assign(mesh.name.begin(), mesh.name.end());
        for (std::size_t i = 0; i < mesh.primitives.size(); ++i) m.primitives.push_back(primCursor++);
        outModel.meshes.emplace_back(std::move(m));
    }
}

} // namespace importers
