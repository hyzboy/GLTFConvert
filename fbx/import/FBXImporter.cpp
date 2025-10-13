#include "fbx/import/FBXImporter.h"

#include <fbxsdk.h>
#include <iostream>
#include <vector>
#include <glm/glm.hpp>
#include "pure/Model.h"
#include "pure/Geometry.h"
#include "pure/Primitive.h"
#include "common/GeometryAttribute.h"
#include "common/PrimitiveType.h"
#include "common/IndexType.h"
#include "common/VkFormat.h"
#include "fbx/import/FBXGeometry.h"
#include "fbx/import/FBXMaterialMap.h"

namespace fbx
{
    void TraverseScene(fbxsdk::FbxScene* scene, FBXModel& model);
    int32_t TraverseNode(fbxsdk::FbxNode* node, FBXModel& model);

    bool ImportFBX(const std::filesystem::path &inputPath, FBXModel &outModel)
    {
        // Initialize FBX SDK
        fbxsdk::FbxManager* lSdkManager = fbxsdk::FbxManager::Create();
        if (!lSdkManager) {
            std::cerr << "Error: Unable to create FBX Manager!\n";
            return false;
        }

        fbxsdk::FbxIOSettings* ios = fbxsdk::FbxIOSettings::Create(lSdkManager, IOSROOT);
        lSdkManager->SetIOSettings(ios);

        fbxsdk::FbxImporter* lImporter = fbxsdk::FbxImporter::Create(lSdkManager, "");

        if (!lImporter->Initialize(inputPath.string().c_str(), -1, lSdkManager->GetIOSettings())) {
            std::cerr << "Error: Unable to initialize FBX importer!\n";
            std::cerr << "Error: " << lImporter->GetStatus().GetErrorString() << "\n";
            return false;
        }

        fbxsdk::FbxScene* lScene = fbxsdk::FbxScene::Create(lSdkManager, "myScene");
        lImporter->Import(lScene);

        // Traverse the scene and fill outModel
        TraverseScene(lScene, outModel);

        // Set source
        outModel.model_source = inputPath.string();

        // Clean up
        lScene->Destroy();
        lImporter->Destroy();
        ios->Destroy();
        lSdkManager->Destroy();

        return true;
    }

    void TraverseScene(fbxsdk::FbxScene* scene, FBXModel& model)
    {
        fbxsdk::FbxNode* root = scene->GetRootNode();
        if (root) {
            std::vector<int32_t> rootChildren;
            for (int i = 0; i < root->GetChildCount(); ++i) {
                int nodeIndex = TraverseNode(root->GetChild(i), model);
                rootChildren.push_back(nodeIndex);
            }
            // Create default scene
            pure::Scene scene;
            scene.name = "default";
            scene.nodes = rootChildren;
            model.scenes.push_back(scene);
        }
    }

    int32_t TraverseNode(fbxsdk::FbxNode* node, FBXModel& model)
    {
        pure::Node n;
        n.name = node->GetName() ? node->GetName() : std::string();
        // TODO: set transform

        // materials
        std::vector<int> materialMap;
        BuildMaterialMap(node, model, materialMap);
        std::vector<int32_t> nodeMaterials;
        for (int mapped : materialMap) {
            if (mapped >= 0) nodeMaterials.push_back(mapped);
        }
        if (!nodeMaterials.empty()) n.materials = nodeMaterials;

        // children
        for (int i = 0; i < node->GetChildCount(); ++i) {
            int childIndex = TraverseNode(node->GetChild(i), model);
            n.children.push_back(childIndex);
        }

        // add node
        int32_t nodeIndex = static_cast<int32_t>(model.nodes.size());
        model.nodes.push_back(n);

        // process mesh after node added
        fbxsdk::FbxMesh* mesh = node->GetMesh();
        if (mesh) {
            size_t meshIndex = model.meshes.size();
            ProcessMesh(mesh, node, model, materialMap);
            if (model.meshes.size() > meshIndex) {
                model.nodes[nodeIndex].mesh = static_cast<int32_t>(meshIndex);
            }
        }

        return nodeIndex;
    }
}
