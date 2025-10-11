#include "gltf/GLTFModel.h"
#include "pure/Model.h"
#include "gltf/convert/UniqueGeometryMapping.h"
#include "gltf/convert/ComputeMeshBounds.h"

namespace gltf
{
    void                    CopyMaterials(                      std::vector<std::unique_ptr<pure::Material>> &     dstMaterials,   const std::vector<GLTFMaterial> &   srcMaterials, const pure::Model &model);
    void                    CopyScenes(                         std::vector<pure::Scene> &        dstScenes,      const std::vector<GLTFScene> &      srcScenes);
    void                    CopyNodes(                          std::vector<pure::Node> &         dstNodes,       const std::vector<GLTFNode> &       srcNodes);
    UniqueGeometryMapping   BuildUniqueGeometryMapping(const    std::vector<GLTFPrimitive> &prims);
    void                    CreateUniqueGeometryEntries(        std::vector<pure::Geometry> &     dstGeometry,    const std::vector<GLTFPrimitive> &  prims,      const gltf::UniqueGeometryMapping &map);
    void                    BuildPrimitives(                    std::vector<pure::Primitive> &    dst,            const std::vector<GLTFPrimitive> &  prims,      const gltf::UniqueGeometryMapping &map);
    void                    BuildMeshes(                        std::vector<pure::Mesh> &         dstMeshes,      const std::vector<GLTFMesh> &       srcMeshes);

    pure::Model ConvertFromGLTF(const GLTFModel &src)
    {
        pure::Model dst;
        dst.gltf_source = src.source;

        // we need images/textures/samplers before collecting references in materials
        dst.images   = src.images;
        dst.textures = src.textures;
        dst.samplers = src.samplers;

        gltf::CopyMaterials(dst.materials, src.materials, dst);
        gltf::CopyScenes(dst.scenes, src.scenes);
        gltf::CopyNodes(dst.nodes, src.nodes);

        auto uniqueMap = gltf::BuildUniqueGeometryMapping(src.primitives);
        gltf::CreateUniqueGeometryEntries(dst.geometry, src.primitives, uniqueMap);
        gltf::BuildPrimitives(dst.primitives, src.primitives, uniqueMap);
        gltf::BuildMeshes(dst.meshes, src.meshes);

        // compute mesh-level bounds from geometry positions
        convert::ComputeMeshBounds(dst);

        return dst;
    }
} // namespace gltf
