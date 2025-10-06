#pragma once
#include <vector>
#include <cstdint>
#include "pure/ModelCore.h"

namespace pure
{
    // Materials
    void CopyMaterials(Model &dst,const GLTFModel &src);
    // Scenes
    void CopyScenes(Model &dst,const GLTFModel &src);
    // Nodes & transforms
    void CopyMeshNodesAndTransforms(Model &dst,const GLTFModel &src);

    // Geometry uniqueness mapping
    struct UniqueGeometryMapping
    {
        std::vector<int32_t> uniqueRepGeomPrimIdx; // representative primitive index for each unique geometry
        std::vector<int32_t> geomIndexOfPrim;      // primitive -> unique geometry index
    };
    UniqueGeometryMapping BuildUniqueGeometryMapping(const GLTFModel &src);
    void CreateUniqueGeometryEntries(Model &dst,const GLTFModel &src,const UniqueGeometryMapping &map);

    // SubMeshes
    void BuildSubMeshes(Model &dst,const GLTFModel &src,const UniqueGeometryMapping &map);

    // Attach subMeshes to nodes
    void AttachNodeSubMeshes(Model &dst,const GLTFModel &src);
}
