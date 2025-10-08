#include "pure/Model.h"
#include "convert/UniqueGeometryMapping.h"
#include "pure/Material.h"
#include "pure/Scene.h"
#include "pure/Geometry.h"
#include "pure/SubMesh.h"
#include "gltf/GLTFModel.h"

namespace pure
{
    void                    CopyMaterials(                      std::vector<Material> &     dstMaterials,   const std::vector<GLTFMaterial> &   srcMaterials);
    void                    CopyScenes(                         std::vector<Scene> &        dstScenes,      const std::vector<GLTFScene> &      srcScenes);
    void                    CopyNodes(                          std::vector<Node> &         dstNodes,       const std::vector<GLTFNode> &       srcNodes);
    UniqueGeometryMapping   BuildUniqueGeometryMapping(const    std::vector<GLTFPrimitive> &prims);
    void                    CreateUniqueGeometryEntries(        std::vector<Geometry> &     dstGeometry,    const std::vector<GLTFPrimitive> &  prims,      const UniqueGeometryMapping &map);
    void                    BuildPrimitives(                    std::vector<Primitive> &    dst,            const std::vector<GLTFPrimitive> &  prims,      const UniqueGeometryMapping &map);
    void                    BuildMeshes(                        std::vector<Mesh> &         dstMeshes,      const std::vector<GLTFMesh> &       srcMeshes);

    Model ConvertFromGLTF(const GLTFModel &src)
    {
        Model dst;
        dst.gltf_source = src.source;

        CopyMaterials(dst.materials, src.materials);
        CopyScenes(dst.scenes, src.scenes);
        CopyNodes(dst.nodes, src.nodes);

        auto uniqueMap = BuildUniqueGeometryMapping(src.primitives);
        CreateUniqueGeometryEntries(dst.geometry, src.primitives, uniqueMap);
        BuildPrimitives(dst.primitives, src.primitives, uniqueMap);
        BuildMeshes(dst.meshes, src.meshes);

        dst.images   = src.images;
        dst.textures = src.textures;
        dst.samplers = src.samplers;
        return dst;
    }
} // namespace pure
