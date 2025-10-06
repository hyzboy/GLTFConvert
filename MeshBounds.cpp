#include "MeshBounds.h"
#include <vector>

namespace pure
{
    void ComputeMeshNodeBounds(pure::Model &model,int32_t nodeIndex)
    {
        auto &node=model.mesh_nodes[static_cast<std::size_t>(nodeIndex)];

        BoundingVolumes nb; // start empty

        if(node.subMeshes.empty()) { node.boundsIndex=model.internBounds(nb); return; }

        const glm::mat4 world=GetNodeWorldMatrix(model,node);

        std::vector<glm::vec3> worldPointsF; worldPointsF.reserve(1024);
        for(auto smIndex:node.subMeshes)
        {
            const auto &sm=model.subMeshes[static_cast<std::size_t>(smIndex)];
            if(sm.geometry==static_cast<std::size_t>(-1)) continue;
            const auto &g=model.geometry[sm.geometry];

            if(g.positions && !g.positions->empty())
            {
                for(const auto &p:*g.positions)
                {
                    glm::vec4 hp = world * glm::vec4(p,1.0f);
                    worldPointsF.emplace_back(hp.x,hp.y,hp.z);
                }
            }
        }

        if(!worldPointsF.empty())
        {
            nb.fromPoints(worldPointsF);
            node.boundsIndex=model.internBounds(nb);
        }
        else
        {
            node.boundsIndex=kInvalidBoundsIndex;
        }
    }

    void ComputeAllMeshNodeBounds(pure::Model &model)
    {
        for(std::size_t ni=0; ni<model.mesh_nodes.size(); ++ni)
        {
            ComputeMeshNodeBounds(model,static_cast<int32_t>(ni));
        }
    }

    void ComputeSceneBounds(pure::Model &model,pure::Scene &scene)
    {
        BoundingVolumes sb; sb.obb.reset(); sb.sphere.reset();

        std::vector<glm::vec3> scenePtsF; scenePtsF.reserve(4096);

        std::vector<int32_t> stack; stack.reserve(scene.nodes.size());
        for(auto v:scene.nodes) stack.push_back(static_cast<int32_t>(v));
        while(!stack.empty())
        {
            auto ni=stack.back(); stack.pop_back();
            const auto &node=model.mesh_nodes[static_cast<std::size_t>(ni)];
            const glm::mat4 world=GetNodeWorldMatrix(model,node);

            for(auto smIndex:node.subMeshes)
            {
                const auto &sm=model.subMeshes[static_cast<std::size_t>(smIndex)];
                if(sm.geometry==static_cast<std::size_t>(-1)) continue;
                const auto &g=model.geometry[sm.geometry];
                if(g.positions && !g.positions->empty())
                {
                    for(const auto &p:*g.positions)
                    {
                        glm::vec4 hp=world*glm::vec4(p,1.0f);
                        scenePtsF.emplace_back(hp.x,hp.y,hp.z);
                    }
                }
            }
            for(auto c:node.children) stack.push_back(static_cast<int32_t>(c));
        }

        if(!scenePtsF.empty())
        {
            sb.fromPoints(scenePtsF);
        }
        else if(scene.boundsIndex!=kInvalidBoundsIndex)
        {
            const auto &existing=model.bounds[static_cast<std::size_t>(scene.boundsIndex)];
            if(!existing.aabb.empty()) sb.sphere=SphereFromAABB(existing.aabb);
        }
        else
        {
            sb.sphere.reset();
        }

        if(scene.boundsIndex!=kInvalidBoundsIndex)
        {
            const auto &existing=model.bounds[static_cast<std::size_t>(scene.boundsIndex)];
            sb.aabb=existing.aabb;
        }

        scene.boundsIndex=model.internBounds(sb);
    }

    void ComputeAllSceneBounds(pure::Model &model)
    {
        for(auto &sc:model.scenes)
        {
            ComputeSceneBounds(model,sc);
        }
    }

} // namespace pure
