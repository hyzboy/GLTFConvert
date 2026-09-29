#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <iostream>
#include <vector>

#include "gltf/GLTFMaterial.h"
#include "gltf/GLTFPrimitive.h"
#include "gltf/GLTFMesh.h"
#include "gltf/GLTFNode.h"
#include "gltf/GLTFScene.h"
#include "gltf/GLTFModel.h"
#include "gltf/import/GLTFExtensions.h"
#include "common/VertexCompression.h"

namespace gltf
{
    static bool g_allowU8Indices = false;
    static pure::NormalExportFormat g_normalExportFormat = pure::NormalExportFormat::V2UN8;
    static bool g_exportTangent = false;
    static bool g_buildMeshlets = true;

    void SetAllowU8Indices(bool allow)
    {
        g_allowU8Indices = allow;
    }

    bool GetAllowU8Indices()
    {
        return g_allowU8Indices;
    }

    void SetNormalExportFormat(pure::NormalExportFormat fmt)
    {
        g_normalExportFormat = fmt;
    }

    pure::NormalExportFormat GetNormalExportFormat()
    {
        return g_normalExportFormat;
    }

    void SetExportTangent(bool allow)
    {
        g_exportTangent = allow;
    }

    bool GetExportTangent()
    {
        return g_exportTangent;
    }

    void SetBuildMeshlets(bool enable)
    {
        g_buildMeshlets = enable;
    }

    bool GetBuildMeshlets()
    {
        return g_buildMeshlets;
    }
    // Forward declarations (headers removed)
    void ImportMaterials(const fastgltf::Asset &asset,std::vector<GLTFMaterial> &materials);
    void ImportPrimitives(const fastgltf::Asset &asset,std::vector<GLTFPrimitive> &primitives);
    void ImportMeshes(const fastgltf::Asset &asset,std::vector<GLTFMesh> &meshes);
    bool ImportNodes(const fastgltf::Asset &asset,std::vector<GLTFNode> &nodes);
    void ImportScenes(const fastgltf::Asset &asset,std::vector<GLTFScene> &scenes);
    void RotatePrimitivesYUpToZUp(std::vector<GLTFPrimitive> &primitives);
    void ImportImages(const fastgltf::Asset &asset,std::vector<GLTFImage> &images);
    void ImportTextures(const fastgltf::Asset &asset,std::vector<GLTFTexture> &textures);
    void ImportSamplers(const fastgltf::Asset &asset,std::vector<GLTFSampler> &samplers);

    bool ImportFastGLTF(const std::filesystem::path &inputPath,GLTFModel &outModel)
    {
        // 先自己预筛必需扩展：fastgltf 的"必需扩展未启用"失败路径在本项目里不干净
        // （实测：打一行错误后进程挂住 >120s 或被 abort、rc=3，且不打印主程序的
        // `[Error] Conversion failed:`）⇒ 这类输入绝不交给它。
        // 启用/拒绝清单见 gltf/import/GLTFExtensions.cpp；"能解析但效果未实现"的给明确告警（不静默降级）。
        const ExtensionCheck extCheck=CheckRequiredExtensions(inputPath);
        if(!extCheck.ok)
        {
            std::cerr<<"[Import] 错误：源文件要求的扩展无法转换 —— "<<extCheck.error
                     <<"；请在 DCC 中关闭该扩展（或预处理）后重新导出\n";
            return false;
        }
        for(const auto &name:extCheck.unhandled)
        {
            std::cerr<<"[Import] 警告：源文件要求扩展 "<<name
                     <<"，转换器已启用解析但**未实现其效果**，导出结果不含该效果\n";
        }

        fastgltf::Parser parser{EnabledExtensions()};
        auto dataRes=fastgltf::GltfDataBuffer::FromPath(inputPath);

        if(dataRes.error()!=fastgltf::Error::None)
        {
            std::cerr<<"[Import] Read failed: "<<fastgltf::getErrorMessage(dataRes.error())<<"\n";
            return false;
        }

        auto data=std::move(dataRes.get());

        constexpr fastgltf::Options options=fastgltf::Options::LoadExternalBuffers
            |fastgltf::Options::LoadGLBBuffers
            |fastgltf::Options::GenerateMeshIndices;

        auto parent=inputPath.parent_path();
        auto assetRes=parser.loadGltf(data,parent,options);

        if(assetRes.error()!=fastgltf::Error::None)
        {
            std::cerr<<"[Import] Parse failed: "<<fastgltf::getErrorMessage(assetRes.error())<<"\n";
            return false;
        }

        fastgltf::Asset asset=std::move(assetRes.get());

        outModel.source=std::filesystem::absolute(inputPath).string();

        ImportMaterials(asset,outModel.materials);
        outModel.primitives.clear();
        outModel.primitives.reserve([&] { std::size_t c=0; for(auto &m:asset.meshes) c+=m.primitives.size(); return c; }());
        ImportPrimitives(asset,outModel.primitives);
        ImportMeshes(asset,outModel.meshes);
        if(!ImportNodes(asset,outModel.nodes))
        {
            std::cerr<<"[Import] 转换中止：节点局部变换无法用 TRS 表示（原因见上），已放弃本次转换\n";
            return false;
        }
        ImportScenes(asset,outModel.scenes);
        outModel.default_scene=static_cast<int32_t>(asset.defaultScene.value_or(0));
        ImportImages(asset,outModel.images);
        ImportTextures(asset,outModel.textures);
        ImportSamplers(asset,outModel.samplers);

        // 节点局部变换的 Y-up→Z-up 转换与镜像感知分解在 ToNodeTransform() 内完成
        // （见 gltf/ToNodeTransform.cpp）；这里只转换图元顶点。
        RotatePrimitivesYUpToZUp(outModel.primitives);
        return true;
    }
} // namespace gltf
