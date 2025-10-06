#include "pure/ModelCore.h"
#include "convert/UniqueGeometryMapping.h"
#include "pure/Material.h"
#include "pure/Scene.h"
#include "pure/MeshNode.h"
#include "pure/Geometry.h"
#include "pure/SubMesh.h"
#include "gltf/Model.h"

namespace pure
{
    // Forward declarations of helper functions now that the central header was removed
    void CopyMaterials(std::vector<Material> &dstMaterials,const std::vector<GLTFMaterial> &srcMaterials);
    void CopyScenes(std::vector<Scene> &dstScenes,const std::vector<GLTFScene> &srcScenes);
    void CopyMeshNodesAndTransforms(std::vector<MeshNode> &dstNodes,const std::vector<GLTFNode> &srcNodes);
    UniqueGeometryMapping BuildUniqueGeometryMapping(const std::vector<GLTFPrimitive> &prims);
    void CreateUniqueGeometryEntries(std::vector<Geometry> &dstGeometry,const std::vector<GLTFPrimitive> &prims,const UniqueGeometryMapping &map);
    void BuildSubMeshes(std::vector<SubMesh> &dstSubMeshes,const std::vector<GLTFPrimitive> &prims,const UniqueGeometryMapping &map);
    void AttachNodeSubMeshes(std::vector<MeshNode> &dstNodes,const std::vector<GLTFNode> &srcNodes,const std::vector<GLTFMesh> &srcMeshes);

    Model ConvertFromGLTF(const GLTFModel &src)
    {
        Model dst; dst.gltf_source=src.source;
        CopyMaterials(dst.materials,src.materials);
        CopyScenes(dst.scenes,src.scenes);
        CopyMeshNodesAndTransforms(dst.mesh_nodes,src.nodes);
        auto uniqueMap=BuildUniqueGeometryMapping(src.primitives);
        CreateUniqueGeometryEntries(dst.geometry,src.primitives,uniqueMap);
        BuildSubMeshes(dst.subMeshes,src.primitives,uniqueMap);
        AttachNodeSubMeshes(dst.mesh_nodes,src.nodes,src.meshes);
        return dst;
    }
} // namespace pure
