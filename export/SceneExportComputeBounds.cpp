#include "SceneExportComputeBounds.h"

#include "SceneExportBounds.h"
#include "pure/Model.h"

namespace exporters
{
    void ComputeBounds(const pure::Model &model,
                       const std::vector<glm::mat4> &worldMatrices,
                       SceneExportData &outData)
    {
        std::vector<glm::vec3> scenePts;
        scenePts.reserve(1024);

        for (auto &n : outData.nodes)
        {
            std::vector<glm::vec3> pts;
            pts.reserve(256);
            GatherWorldPoints(model, n.originalIndex, worldMatrices, pts);
            if (!pts.empty())
            {
                n.boundsIndex = AddBounds(outData.boundsTable, pts);
                scenePts.insert(scenePts.end(), pts.begin(), pts.end());
            }
            else
            {
                n.boundsIndex = -1;
            }
        }

        if (!scenePts.empty())
            outData.sceneBoundsIndex = AddBounds(outData.boundsTable, scenePts);
        else
            outData.sceneBoundsIndex = -1;
    }
}
