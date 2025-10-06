#include "ImportMeshes.h"

namespace importers {

void ImportMeshes(const fastgltf::Asset& asset, std::vector<GLTFMesh>& meshes)
{
    std::size_t primCursor = 0;
    meshes.clear();
    meshes.reserve(asset.meshes.size());
    for (const auto& mesh : asset.meshes) {
        GLTFMesh m{};
        if (!mesh.name.empty()) m.name.assign(mesh.name.begin(), mesh.name.end());
        for (std::size_t i = 0; i < mesh.primitives.size(); ++i) m.primitives.push_back(primCursor++);
        meshes.emplace_back(std::move(m));
    }
}

} // namespace importers
