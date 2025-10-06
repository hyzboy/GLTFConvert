#include <vector>

#include "pure/Scene.h"
#include "gltf/Scene.h"

namespace pure
{
    void CopyScenes(std::vector<Scene> &dstScenes, const std::vector<GLTFScene> &srcScenes)
    {
        dstScenes.reserve(srcScenes.size());
        for (const auto &s : srcScenes)
        {
            Scene ps;
            ps.name = s.name;
            ps.nodes.reserve(s.nodes.size());
            for (auto ni : s.nodes)
                ps.nodes.push_back(static_cast<int32_t>(ni));
            dstScenes.push_back(std::move(ps));
        }
    }
} // namespace pure
