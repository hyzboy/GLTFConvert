#include <fastgltf/core.hpp>
#include <vector>
#include "gltf/GLTFSampler.h"

namespace gltf
{
    void ImportSamplers(const fastgltf::Asset &asset,std::vector<GLTFSampler> &samplers)
    {
        samplers.clear();
        samplers.resize(asset.samplers.size());
        for(std::size_t i=0;i<asset.samplers.size();++i)
        {
            const auto &s = asset.samplers[i];
            auto &os = samplers[i];
            os.wrapS = static_cast<int>(s.wrapS);
            os.wrapT = static_cast<int>(s.wrapT);
            if(s.magFilter) os.magFilter = static_cast<int>(*s.magFilter);
            if(s.minFilter) os.minFilter = static_cast<int>(*s.minFilter);
            if(!s.name.empty()) os.name.assign(s.name.begin(), s.name.end());
        }
    }
}
