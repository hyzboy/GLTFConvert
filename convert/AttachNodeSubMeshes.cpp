#include <vector>

#include "pure/MeshNode.h"
#include "gltf/GLTFNode.h"
#include "gltf/GLTFMesh.h"

namespace pure
{
    void AttachNodeSubMeshes(std::vector<MeshNode> &dstNodes, const std::vector<GLTFNode> &srcNodes, const std::vector<GLTFMesh> &srcMeshes)
    {
        for (int32_t ni = 0; ni < static_cast<int32_t>(srcNodes.size()); ++ni)
        {
            const auto &node = srcNodes[static_cast<size_t>(ni)];
            auto &pn = dstNodes[static_cast<size_t>(ni)];
            if (node.mesh)
            {
                const auto &mesh = srcMeshes[static_cast<size_t>(*node.mesh)];
                pn.subMeshes.reserve(mesh.primitives.size());
                for (auto primIndex : mesh.primitives)
                    pn.subMeshes.push_back(static_cast<int32_t>(primIndex));
            }
        }
    }
} // namespace pure
