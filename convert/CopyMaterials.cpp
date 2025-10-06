#include "convert/ConvertInternals.h"

namespace pure
{
    void CopyMaterials(Model &dst,const GLTFModel &src)
    {
        dst.materials.reserve(src.materials.size());
        for(const auto &m:src.materials)
        {
            Material pm; pm.name=m.name; dst.materials.push_back(std::move(pm));
        }
    }
}
