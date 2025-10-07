#include "SceneExportNodes.h"

#include "pure/Model.h"
#include "pure/MeshNode.h"
#include "pure/SubMesh.h"

namespace exporters
{
    void BuildNodes(const pure::Model &model,
                    const CollectedIndices &ci,
                    const RemapTables &remap,
                    const std::vector<glm::mat4> &worldMatrices,
                    std::unordered_map<std::string, int32_t> &nameToIndex,
                    SceneExportData &outData)
    {
        outData.nodes.reserve(ci.nodes.size());

        for (int32_t originalNode : ci.nodes)
        {
            const auto &src = model.mesh_nodes[originalNode];

            SceneNodeExport ne;
            ne.originalIndex     = originalNode;
            ne.nameIndex         = GetOrAddName(nameToIndex, outData.nameTable, src.name);

            glm::mat4 localM     = src.node_transform.rawMat4();
            ne.localMatrixIndex  = GetOrAddMatrix(outData.matrixTable, localM);
            glm::mat4 worldM     = worldMatrices[originalNode];
            ne.worldMatrixIndex  = GetOrAddMatrix(outData.matrixTable, worldM);

            if (src.node_transform.isTRS())
                ne.trsIndex = GetOrAddTRS(outData.trsTable, src.node_transform.trs);

            ne.subMeshes.reserve(src.subMeshes.size());
            for (int32_t sm : src.subMeshes)
            {
                auto it = remap.subMeshRemap.find(sm);
                if (it != remap.subMeshRemap.end())
                    ne.subMeshes.push_back(it->second);
            }

            ne.children.reserve(src.children.size());
            for (int32_t c : src.children)
            {
                auto it = remap.nodeRemap.find(c);
                if (it != remap.nodeRemap.end())
                    ne.children.push_back(it->second);
            }

            outData.nodes.push_back(std::move(ne));
        }
    }
}
