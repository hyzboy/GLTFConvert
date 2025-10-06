#include "convert/ConvertInternals.h"

namespace pure
{
    void CopyScenes(Model &dst,const GLTFModel &src)
    {
        dst.scenes.reserve(src.scenes.size());
        for(const auto &s:src.scenes)
        {
            Scene ps; ps.name=s.name; ps.nodes.reserve(s.nodes.size());
            for(auto ni:s.nodes) ps.nodes.push_back(static_cast<int32_t>(ni));
            dst.scenes.push_back(std::move(ps));
        }
    }
}
