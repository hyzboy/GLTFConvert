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

namespace fbx
{
    void TraverseScene(fbxsdk::FbxScene* scene, FBXModel& model);
    void TraverseNode(fbxsdk::FbxNode* node, FBXModel& model);

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
            for (int i = 0; i < root->GetChildCount(); ++i) {
                TraverseNode(root->GetChild(i), model);
            }
        }
    }

    void TraverseNode(fbxsdk::FbxNode* node, FBXModel& model)
    {
        fbxsdk::FbxMesh* mesh = node->GetMesh();
        if (mesh) {
            ProcessMesh(mesh, node, model);
        }

        for (int i = 0; i < node->GetChildCount(); ++i) {
            TraverseNode(node->GetChild(i), model);
        }
    }

    // ProcessMesh implementation moved to fbx/import/FBXGeometry.cpp
}
