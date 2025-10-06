#include <vector>
#include "gltf/Node.h"

namespace importers
{
    void RotateNodeLocalTransformsYUpToZUp(std::vector<GLTFNode> &nodes)
    {
        for(auto &n:nodes)
        {
            n.transform.convertInPlaceYUpToZUp();
        }
    }
} // namespace importers
