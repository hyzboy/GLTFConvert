#include <fastgltf/core.hpp>
#include <vector>
#include "gltf/GLTFTexture.h"

namespace gltf
{
    void ImportTextures(const fastgltf::Asset &asset,std::vector<GLTFTexture> &textures)
    {
        textures.clear();
        textures.resize(asset.textures.size());
        for(std::size_t i=0;i<asset.textures.size();++i)
        {
            const auto &t = asset.textures[i];
            auto &ot = textures[i];
            if(t.imageIndex) ot.image = t.imageIndex; // optional
            if(t.samplerIndex) ot.sampler = t.samplerIndex;
            if(!t.name.empty()) ot.name.assign(t.name.begin(), t.name.end());
        }
    }
}
