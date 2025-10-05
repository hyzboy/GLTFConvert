#include "MeshBounds.h"

#include <vector>

namespace pure
{

    // local helper: decode POSITION to float3
    static bool TryDecodePositionsAsVec3(const GLTFGeometry &g,std::vector<glm::vec3> &out)
    {
        out.clear();
        for(const auto &a:g.attributes)
        {
            if(a.name=="POSITION"&&a.componentType=="FLOAT"&&(a.type=="VEC3"||a.type=="VEC4"))
            {
                // assume tightly packed float3/float4
                const std::byte *ptr=a.data.data();
                const std::size_t elementCount=a.count;
                const std::size_t stride=(a.type=="VEC3")?sizeof(float)*3:sizeof(float)*4;
                if(a.data.size()<elementCount*stride) return false;
                out.resize(elementCount);
                for(std::size_t i=0; i<elementCount; ++i)
                {
                    const float *f=reinterpret_cast<const float *>(ptr+i*stride);
                    out[i]=glm::vec3(f[0],f[1],f[2]);
                }
                return true;
            }
        }
        return false;
    }

    void ComputeGeometryBoundsFromGLTF(pure::Model &model,const GLTFGeometry &srcGeom,pure::Geometry &dstGeom)
    {
        BoundingBox bb; // local aggregate
        bb.aabb=srcGeom.localAABB; // AABB comes from glTF primitive local AABB

        // Decode positions once (float) and compute OBB and sphere
        std::vector<glm::vec3> posF;
        bool hasPos = TryDecodePositionsAsVec3(srcGeom,posF) && !posF.empty();
        if(hasPos)
        {
            dstGeom.positions = posF; // cache local-space positions (float)

            // Build OBB directly from float points
            bb.obb = OBB::fromPointsMinVolume(posF);
            bb.sphere = SphereFromPoints(posF);
        }
        else
        {
            dstGeom.positions.reset();
            bb.obb.reset();
            bb.sphere.reset();
        }

        if(!hasPos && !bb.aabb.empty())
        {
            bb.sphere = SphereFromAABB(bb.aabb);
        }

        dstGeom.boundsIndex=model.internBounds(bb);
    }

    void ComputeMeshNodeBounds(pure::Model &model,int32_t nodeIndex)
    {
        auto &node=model.mesh_nodes[static_cast<std::size_t>(nodeIndex)];

        BoundingBox nb; // start empty
        nb.aabb.reset();
        nb.obb.reset();
        nb.sphere.reset();
        if(node.subMeshes.empty()) { node.boundsIndex=model.internBounds(nb); return; }

        const glm::mat4 world=GetNodeWorldMatrix(model,node);

        std::vector<glm::vec3> worldPointsF; worldPointsF.reserve(1024);
        for(auto smIndex:node.subMeshes)
        {
            const auto &sm=model.subMeshes[static_cast<std::size_t>(smIndex)];
            if(sm.geometry==static_cast<std::size_t>(-1)) continue;
            const auto &g=model.geometry[sm.geometry];
            if(g.boundsIndex!=kInvalidBoundsIndex)
            {
                const BoundingBox &gb=model.bounds[static_cast<std::size_t>(g.boundsIndex)];
                if(!gb.aabb.empty())
                {
                    nb.aabb.merge(gb.aabb.transformed(world));
                }
            }
            // collect transformed positions using cached (float) positions
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
            nb.obb=OBB::fromPointsMinVolume(worldPointsF);
            nb.sphere=SphereFromPoints(worldPointsF);
        }
        else if(!nb.aabb.empty())
        {
            nb.sphere=SphereFromAABB(nb.aabb);
        }

        node.boundsIndex=model.internBounds(nb);
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
        BoundingBox sb; sb.obb.reset(); sb.sphere.reset();

        std::vector<glm::vec3> scenePtsF; scenePtsF.reserve(4096);

        // traverse nodes of scene and gather each node's world-space points again
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
            sb.obb=OBB::fromPointsMinVolume(scenePtsF);
            sb.sphere=SphereFromPoints(scenePtsF);
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

        // Keep existing AABB if any (copied from glTF at scene creation)
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
