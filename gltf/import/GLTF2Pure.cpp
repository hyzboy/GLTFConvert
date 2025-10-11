#include "gltf/GLTFModel.h"
#include "gltf/GLTFTextureInfo.h"
#include "pure/Model.h"
#include "gltf/convert/UniqueGeometryMapping.h"
#include "gltf/convert/ComputeMeshBounds.h"

namespace gltf
{
    void                    CopyMaterials(                      std::vector<std::unique_ptr<pure::Material>> &     dstMaterials,   const std::vector<GLTFMaterial> &   srcMaterials, const pure::Model &model);
    void                    CopyScenes(                         std::vector<pure::Scene> &        dstScenes,      const std::vector<GLTFScene> &      srcScenes);
    void                    CopyNodes(                          std::vector<pure::Node> &         dstNodes,       const std::vector<GLTFNode> &       srcNodes);
    void                    CopySamplers(                       std::vector<pure::Sampler> &      dstSamplers,    const std::vector<GLTFSampler> &    srcSamplers);
    void                    CopyImages(                         std::vector<pure::Image> &        dstImages,      const std::vector<GLTFImage> &      srcImages);
    void                    CopyTextures(                       std::vector<pure::Texture> &      dstTextures,    const std::vector<GLTFTexture> &    srcTextures);
    UniqueGeometryMapping   BuildUniqueGeometryMapping(const    std::vector<GLTFPrimitive> &prims);
    void                    CreateUniqueGeometryEntries(        std::vector<pure::Geometry> &     dstGeometry,    const std::vector<GLTFPrimitive> &  prims,      const gltf::UniqueGeometryMapping &map);
    void                    BuildPrimitives(                    std::vector<pure::Primitive> &    dst,            const std::vector<GLTFPrimitive> &  prims,      const gltf::UniqueGeometryMapping &map);
    void                    BuildMeshes(                        std::vector<pure::Mesh> &         dstMeshes,      const std::vector<GLTFMesh> &       srcMeshes);

    pure::Model ConvertFromGLTF(const GLTFModel &src)
    {
        pure::Model dst;
        dst.gltf_source = src.source;

        // we need images/textures/samplers before collecting references in materials
        gltf::CopyImages(dst.images, src.images);
        gltf::CopyTextures(dst.textures, src.textures);
        gltf::CopySamplers(dst.samplers, src.samplers);

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

    void CopySamplers(std::vector<pure::Sampler> &dstSamplers, const std::vector<GLTFSampler> &srcSamplers)
    {
        dstSamplers.reserve(srcSamplers.size());
        for (const auto &s : srcSamplers)
        {
            pure::Sampler ps;
            ps.wrapS = static_cast<pure::WrapMode>(s.wrapS);
            ps.wrapT = static_cast<pure::WrapMode>(s.wrapT);
            ps.magFilter = s.magFilter ? std::optional<pure::FilterMode>(static_cast<pure::FilterMode>(*s.magFilter)) : std::nullopt;
            ps.minFilter = s.minFilter ? std::optional<pure::FilterMode>(static_cast<pure::FilterMode>(*s.minFilter)) : std::nullopt;
            dstSamplers.push_back(std::move(ps));
        }
    }

    void CopyImages(std::vector<pure::Image> &dstImages, const std::vector<GLTFImage> &srcImages)
    {
        dstImages.reserve(srcImages.size());
        for (const auto &i : srcImages)
        {
            pure::Image pi;
            pi.name = i.name;
            pi.mimeType = i.mimeType;
            pi.data = i.data;
            pi.uri = i.uri;
            dstImages.push_back(std::move(pi));
        }
    }

    void CopyTextures(std::vector<pure::Texture> &dstTextures, const std::vector<GLTFTexture> &srcTextures)
    {
        dstTextures.reserve(srcTextures.size());
        for (const auto &t : srcTextures)
        {
            pure::Texture pt;
            pt.sampler = t.sampler;
            pt.image = t.image;
            dstTextures.push_back(std::move(pt));
        }
    }
} // namespace gltf
