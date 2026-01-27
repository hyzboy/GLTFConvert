#include <vector>
#include "gltf/GLTFNode.h"

namespace gltf
{
    void RotateNodeLocalTransformsYUpToZUp(std::vector<GLTFNode> &nodes)
    {
        for(auto &n: nodes)
        {
            n.transform.convertInPlaceYUpToZUp();
        }
    }
} // namespace gltf
