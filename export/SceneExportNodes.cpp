#include "SceneExportNodes.h"

#include "pure/Model.h"
#include "pure/Node.h"
#include "pure/Mesh.h"
#include "pure/Primitive.h"

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
            const auto &src = model.nodes[originalNode];

            SceneNodeExport ne;
            ne.originalIndex     = originalNode;
            ne.nameIndex         = GetOrAddName(nameToIndex, outData.nameTable, src.name);

            glm::mat4 localM     = src.transform.rawMat4();
            ne.localMatrixIndex  = GetOrAddMatrix(outData.matrixTable, localM);
            glm::mat4 worldM     = worldMatrices[originalNode];
            ne.worldMatrixIndex  = GetOrAddMatrix(outData.matrixTable, worldM);

            if (src.transform.isTRS())
                ne.trsIndex = GetOrAddTRS(outData.trsTable, src.transform.trs);

            if (src.mesh && *src.mesh >= 0 && *src.mesh < static_cast<int32_t>(model.meshes.size()))
            {
                const auto &mesh = model.meshes[*src.mesh];
                ne.primitives.reserve(mesh.primitives.size());
                for (int32_t pm : mesh.primitives)
                {
                    auto it = remap.primitiveRemap.find(pm);
                    if (it != remap.primitiveRemap.end())
                        ne.primitives.push_back(it->second);
                }
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
