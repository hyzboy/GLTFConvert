#include "Importer.h"

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <iostream>
#include <fstream>
#include <cstring>

#include "common/FastGLTFConversions.h"

namespace importers
{
    namespace
    {
        // ---- Buffer helpers --------------------------------------------------------
        static bool CopyAccessorToBytes(const fastgltf::Asset& asset,
                                        const fastgltf::Accessor& accessor,
                                        std::vector<std::byte>& out)
        {
            if (!accessor.bufferViewIndex)
                return false;

            const auto& bv = asset.bufferViews[*accessor.bufferViewIndex];
            if (bv.bufferIndex >= asset.buffers.size())
                return false;

            const auto& buf = asset.buffers[bv.bufferIndex];

            size_t elemSize   = fastgltf::getElementByteSize(accessor.type, accessor.componentType);
            size_t totalBytes = elemSize * accessor.count;

            bool ok = false;
            std::visit(fastgltf::visitor {
                [&](const fastgltf::sources::Vector& vec)
                {
                    if (bv.byteOffset + accessor.byteOffset + totalBytes > vec.bytes.size()) return;
                    const std::byte* src = vec.bytes.data() + bv.byteOffset + accessor.byteOffset;
                    out.resize(totalBytes);
                    std::memcpy(out.data(), src, totalBytes);
                    ok = true;
                },
                [&](const fastgltf::sources::Array& arr)
                {
                    if (bv.byteOffset + accessor.byteOffset + totalBytes > arr.bytes.size()) return;
                    const std::byte* src = arr.bytes.data() + bv.byteOffset + accessor.byteOffset;
                    out.resize(totalBytes);
                    std::memcpy(out.data(), src, totalBytes);
                    ok = true;
                },
                [&](const fastgltf::sources::ByteView& bvw)
                {
                    if (bv.byteOffset + accessor.byteOffset + totalBytes > bvw.bytes.size()) return;
                    const std::byte* src = bvw.bytes.data() + bv.byteOffset + accessor.byteOffset;
                    out.resize(totalBytes);
                    std::memcpy(out.data(), src, totalBytes);
                    ok = true;
                },
                [&](const auto&)
                {
                }
            }, buf.data);

            return ok;
        }

        // ---- Math/helpers & orientation change ------------------------------------
        static std::optional<std::pair<glm::dvec3, glm::dvec3>>
        ComputeAABBFromAccessorFloat(const fastgltf::Asset& asset, const fastgltf::Accessor& acc)
        {
            if (acc.type != fastgltf::AccessorType::Vec3)
                return std::nullopt;
            if (acc.componentType != fastgltf::ComponentType::Float &&
                acc.componentType != fastgltf::ComponentType::Double)
                return std::nullopt;

            glm::dvec3 mn(std::numeric_limits<double>::infinity());
            glm::dvec3 mx(-std::numeric_limits<double>::infinity());
            bool       any = false;

            if (acc.componentType == fastgltf::ComponentType::Float)
            {
                fastgltf::iterateAccessor<fastgltf::math::fvec3>(asset, acc,
                    [&](const fastgltf::math::fvec3& v)
                    {
                        any = true;
                        mn  = glm::min(mn, glm::dvec3(v.x(), v.y(), v.z()));
                        mx  = glm::max(mx, glm::dvec3(v.x(), v.y(), v.z()));
                    });
            }
            else
            {
                fastgltf::iterateAccessor<fastgltf::math::dvec3>(asset, acc,
                    [&](const fastgltf::math::dvec3& v)
                    {
                        any = true;
                        mn  = glm::min(mn, glm::dvec3(v.x(), v.y(), v.z()));
                        mx  = glm::max(mx, glm::dvec3(v.x(), v.y(), v.z()));
                    });
            }

            if (!any)
                return std::nullopt;

            return std::make_pair(mn, mx);
        }

        // Rotate Y-up to Z-up: x' = x, y' = -z, z' = y
        inline void RotateYUpToZUp(float& x, float& y, float& z)
        {
            float nx = x;
            float ny = -z;
            float nz = y;
            x = nx; y = ny; z = nz;
        }
        inline void RotateYUpToZUp(double& x, double& y, double& z)
        {
            double nx = x;
            double ny = -z;
            double nz = y;
            x = nx; y = ny; z = nz;
        }

        static void ApplyYUpToZUp(GLTFModel& model)
        {
            glm::dmat4 rot(1.0);
            rot = glm::rotate(rot, glm::radians(90.0), glm::dvec3(1.0, 0.0, 0.0));

            for (auto& prim : model.primitives)
            {
                auto& geom = prim.geometry;
                for (auto& attr : geom.attributes)
                {
                    const std::string& name = attr.name;
                    if (name == "POSITION" || name == "NORMAL" || name == "TANGENT" || name == "BITANGENT")
                    {
                        if (attr.data.empty() || attr.count == 0)
                            continue;

                        if (name == "TANGENT")
                        {
                            if (attr.format == PF_RGBA32F)
                            {
                                float* f = reinterpret_cast<float*>(attr.data.data());
                                for (std::size_t i = 0; i < attr.count; ++i)
                                {
                                    float& x = f[i * 4 + 0];
                                    float& y = f[i * 4 + 1];
                                    float& z = f[i * 4 + 2];
                                    RotateYUpToZUp(x, y, z);
                                }
                            }
                            else if (attr.format == PF_RGBA64F)
                            {
                                double* d = reinterpret_cast<double*>(attr.data.data());
                                for (std::size_t i = 0; i < attr.count; ++i)
                                {
                                    double& x = d[i * 4 + 0];
                                    double& y = d[i * 4 + 1];
                                    double& z = d[i * 4 + 2];
                                    RotateYUpToZUp(x, y, z);
                                }
                            }
                        }
                        else if (name == "POSITION" || name == "NORMAL" || name == "BITANGENT")
                        {
                            if (attr.format == PF_RGB32F)
                            {
                                float* f = reinterpret_cast<float*>(attr.data.data());
                                for (std::size_t i = 0; i < attr.count; ++i)
                                {
                                    float& x = f[i * 3 + 0];
                                    float& y = f[i * 3 + 1];
                                    float& z = f[i * 3 + 2];
                                    RotateYUpToZUp(x, y, z);
                                }
                            }
                            else if (attr.format == PF_RGB64F)
                            {
                                double* d = reinterpret_cast<double*>(attr.data.data());
                                for (std::size_t i = 0; i < attr.count; ++i)
                                {
                                    double& x = d[i * 3 + 0];
                                    double& y = d[i * 3 + 1];
                                    double& z = d[i * 3 + 2];
                                    RotateYUpToZUp(x, y, z);
                                }
                            }
                        }
                    }
                }

                if (!geom.localAABB.empty())
                {
                    geom.localAABB = geom.localAABB.transformed(rot);
                }
            }
        }

        static void ApplyYUpToZUpNodeTransforms(GLTFModel& model)
        {
            for (auto& n : model.nodes)
            {
                n.transform.convertInPlaceYUpToZUp();
            }
        }

        // ---- Import helpers --------------------------------------------------------
        static void ImportMaterials(const fastgltf::Asset& asset, GLTFModel& outModel)
        {
            outModel.materials.clear();
            outModel.materials.reserve(asset.materials.size());
            for (const auto& m : asset.materials)
            {
                GLTFMaterial om {};
                if (!m.name.empty())
                    om.name.assign(m.name.begin(), m.name.end());
                outModel.materials.emplace_back(std::move(om));
            }
        }

        static void ImportPrimitives(const fastgltf::Asset& asset, GLTFModel& outModel)
        {
            for (const auto& mesh : asset.meshes)
            {
                for (const auto& prim : mesh.primitives)
                {
                    GLTFPrimitive p {};
                    p.geometry.primitiveType = FastGLTFModeToPrimitiveType(prim.type);

                    // attributes
                    for (const auto& attr : prim.attributes)
                    {
                        const auto& acc = asset.accessors[attr.accessorIndex];
                        GLTFGeometry::GLTFGeometryAttribute a {};
                        a.name          = attr.name;
                        a.count         = acc.count;
                        a.format        = FastGLTFAccessorTypeToVkFormat(acc.type, acc.componentType);
                        a.accessorIndex = attr.accessorIndex;
                        if (!CopyAccessorToBytes(asset, acc, a.data))
                            a.data.clear();
                        p.geometry.attributes.emplace_back(std::move(a));

                        if (p.geometry.attributes.back().name == "POSITION")
                        {
                            if (auto mm = ComputeAABBFromAccessorFloat(asset, acc))
                            {
                                p.geometry.localAABB.min = mm->first;
                                p.geometry.localAABB.max = mm->second;
                            }
                        }
                    }

                    // indices
                    if (prim.indicesAccessor)
                    {
                        const auto& acc = asset.accessors[*prim.indicesAccessor];
                        std::vector<std::byte> buf;
                        if (CopyAccessorToBytes(asset, acc, buf))
                        {
                            p.geometry.indices                = std::move(buf);
                            p.geometry.indexCount             = acc.count;
                            p.geometry.indicesAccessorIndex   = prim.indicesAccessor;
                            p.geometry.indexType              = FastGLTFComponentTypeToIndexType(acc.componentType);
                        }
                    }

                    // material
                    if (prim.materialIndex)
                        p.material = *prim.materialIndex;

                    outModel.primitives.emplace_back(std::move(p));
                }
            }
        }

        static void ImportMeshes(const fastgltf::Asset& asset, GLTFModel& outModel)
        {
            std::size_t primCursor = 0;
            outModel.meshes.clear();
            outModel.meshes.reserve(asset.meshes.size());
            for (const auto& mesh : asset.meshes)
            {
                GLTFMesh m {};
                if (!mesh.name.empty())
                    m.name.assign(mesh.name.begin(), mesh.name.end());
                for (std::size_t i = 0; i < mesh.primitives.size(); ++i)
                    m.primitives.push_back(primCursor++);
                outModel.meshes.emplace_back(std::move(m));
            }
        }

        static void ImportNodes(const fastgltf::Asset& asset, GLTFModel& outModel)
        {
            outModel.nodes.resize(asset.nodes.size());
            for (std::size_t i = 0; i < asset.nodes.size(); ++i)
            {
                const auto& n  = asset.nodes[i];
                auto&       on = outModel.nodes[i];
                if (!n.name.empty())
                    on.name.assign(n.name.begin(), n.name.end());
                if (n.meshIndex)
                    on.mesh = *n.meshIndex;
                on.children.assign(n.children.begin(), n.children.end());
                on.transform = n.transform;
            }
        }

        static void ImportScenes(const fastgltf::Asset& asset, GLTFModel& outModel)
        {
            outModel.scenes.resize(asset.scenes.size());
            for (std::size_t i = 0; i < asset.scenes.size(); ++i)
            {
                const auto& sc = asset.scenes[i];
                auto&       os = outModel.scenes[i];
                if (!sc.name.empty())
                    os.name.assign(sc.name.begin(), sc.name.end());
                os.nodes.assign(sc.nodeIndices.begin(), sc.nodeIndices.end());
            }
        }
    }

    bool ImportFastGLTF(const std::filesystem::path& inputPath, GLTFModel& outModel)
    {
        fastgltf::Parser parser {};
        auto dataRes = fastgltf::GltfDataBuffer::FromPath(inputPath);
        if (dataRes.error() != fastgltf::Error::None)
        {
            std::cerr << "[Import] Read failed: " << fastgltf::getErrorMessage(dataRes.error()) << "\n";
            return false;
        }
        auto data = std::move(dataRes.get());

        constexpr fastgltf::Options options = fastgltf::Options::LoadExternalBuffers
                                            | fastgltf::Options::LoadGLBBuffers
                                            | fastgltf::Options::DecomposeNodeMatrices
                                            | fastgltf::Options::GenerateMeshIndices;

        auto parent   = inputPath.parent_path();
        auto assetRes = parser.loadGltf(data, parent, options);
        if (assetRes.error() != fastgltf::Error::None)
        {
            std::cerr << "[Import] Parse failed: " << fastgltf::getErrorMessage(assetRes.error()) << "\n";
            return false;
        }
        fastgltf::Asset asset = std::move(assetRes.get());

        outModel.source = std::filesystem::absolute(inputPath).string();

        // Import sequence
        ImportMaterials(asset, outModel);

        outModel.primitives.clear();
        outModel.primitives.reserve([&]
        {
            std::size_t c = 0;
            for (auto& m : asset.meshes) c += m.primitives.size();
            return c;
        }());
        ImportPrimitives(asset, outModel);
        ImportMeshes(asset, outModel);
        ImportNodes(asset, outModel);
        ImportScenes(asset, outModel);

        // Orientation adjustments
        ApplyYUpToZUpNodeTransforms(outModel);
        ApplyYUpToZUp(outModel);

        return true;
    }
}//namespace importers
