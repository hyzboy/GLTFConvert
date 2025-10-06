#include "pure/ModelCore.h"
#include "pure/Material.h"
#include "pure/MeshNode.h"
#include "pure/Scene.h"
#include <algorithm>

namespace pure
{
    namespace
    {
        // Compare geometries by their accessor indices and attributes
        static bool SameGeometryByAccessorId(const GLTFGeometry &a,const GLTFGeometry &b)
        {
            if(a.mode!=b.mode) return false;
            if(static_cast<bool>(a.indicesAccessorIndex)!=static_cast<bool>(b.indicesAccessorIndex)) return false;
            if(a.indicesAccessorIndex&&b.indicesAccessorIndex&&(*a.indicesAccessorIndex!=*b.indicesAccessorIndex)) return false;
            if(a.attributes.size()!=b.attributes.size()) return false;

            auto makeList=[](const GLTFGeometry &g)
                {
                    std::vector<std::pair<std::string,std::size_t>> v;
                    v.reserve(g.attributes.size());
                    for(const auto &at:g.attributes)
                    {
                        if(!at.accessorIndex) return std::vector<std::pair<std::string,std::size_t>>{};
                        v.emplace_back(at.name,*at.accessorIndex);
                    }
                    std::sort(v.begin(),v.end(),[](auto &l,auto &r)
                              {
                                  if(l.first!=r.first) return l.first<r.first;
                                  return l.second<r.second;
                              });
                    return v;
                };

            auto la=makeList(a);
            auto lb=makeList(b);
            if(la.empty()||lb.empty()) return false;
            return la==lb;
        }

        static void CopyMaterials(Model &dst,const GLTFModel &src)
        {
            dst.materials.reserve(src.materials.size());
            for(const auto &m:src.materials)
            {
                Material pm;
                pm.name=m.name;
                dst.materials.push_back(std::move(pm));
            }
        }

        static void CopyScenes(Model &dst,const GLTFModel &src)
        {
            dst.scenes.reserve(src.scenes.size());
            for(const auto &s:src.scenes)
            {
                Scene ps;
                ps.name=s.name;
                ps.nodes.reserve(s.nodes.size());
                for(auto ni:s.nodes) ps.nodes.push_back(static_cast<int32_t>(ni));
                dst.scenes.push_back(std::move(ps));
            }
        }

        static void CopyMeshNodesAndTransforms(Model &dst,const GLTFModel &src)
        {
            dst.mesh_nodes.reserve(src.nodes.size());
            for(const auto &n:src.nodes)
            {
                MeshNode pn;

                pn.name=n.name;
                pn.children.reserve(n.children.size());
                for(auto c:n.children) pn.children.push_back(static_cast<int32_t>(c));

                pn.node_transform=n.transform;

                dst.mesh_nodes.push_back(std::move(pn));
            }
        }

        struct UniqueGeometryMapping
        {
            std::vector<int32_t> uniqueRepGeomPrimIdx; // primitive index that represents each unique geometry
            std::vector<int32_t> geomIndexOfPrim;      // maps primitive -> unique geometry index
        };

        static UniqueGeometryMapping BuildUniqueGeometryMapping(const GLTFModel &src)
        {
            UniqueGeometryMapping map;
            const auto &prims=src.primitives;
            map.geomIndexOfPrim.assign(prims.size(),-1);

            for(int32_t i=0; i<static_cast<int32_t>(prims.size()); ++i)
            {
                const auto &g=prims[static_cast<size_t>(i)].geometry;
                int32_t found=-1;
                for(int32_t u=0; u<static_cast<int32_t>(map.uniqueRepGeomPrimIdx.size()); ++u)
                {
                    if(SameGeometryByAccessorId(g,prims[static_cast<size_t>(map.uniqueRepGeomPrimIdx[static_cast<size_t>(u)])].geometry))
                    {
                        found=u;
                        break;
                    }
                }
                if(found==-1)
                {
                    map.uniqueRepGeomPrimIdx.push_back(i);
                    map.geomIndexOfPrim[static_cast<size_t>(i)]=static_cast<int32_t>(map.uniqueRepGeomPrimIdx.size())-1;
                }
                else
                {
                    map.geomIndexOfPrim[static_cast<size_t>(i)]=found;
                }
            }
            return map;
        }

        static void CreateUniqueGeometryEntries(Model &dst,const GLTFModel &src,const UniqueGeometryMapping &map)
        {
            const auto &prims=src.primitives;
            dst.geometry.reserve(map.uniqueRepGeomPrimIdx.size());

            for(int32_t u=0; u<static_cast<int32_t>(map.uniqueRepGeomPrimIdx.size()); ++u)
            {
                int32_t i=map.uniqueRepGeomPrimIdx[static_cast<size_t>(u)];
                const auto &g=prims[static_cast<size_t>(i)].geometry;

                Geometry pg;
                pg.mode=g.mode;
                pg.attributes.reserve(g.attributes.size());

                for(size_t ai=0; ai<g.attributes.size(); ++ai)
                {
                    const auto &a=g.attributes[ai];
                    GeometryAttribute ga;
                    ga.id=static_cast<uint8_t>(ai);
                    ga.name=a.name;
                    ga.count=a.count;
                    ga.componentType=a.componentType;
                    ga.type=a.type;
                    ga.format=a.format;
                    ga.data=a.data;
                    pg.attributes.push_back(std::move(ga));
                }

                if(g.indices) pg.indicesData=*g.indices;
                if(g.indexCount&&g.indexComponentType)
                {
                    pg.indices=GeometryIndicesMeta{ *g.indexCount,*g.indexComponentType };
                    pg.indices->indexType=g.indexType;
                }

                dst.geometry.push_back(std::move(pg));
            }
        }

        static void BuildSubMeshes(Model &dst,const GLTFModel &src,const UniqueGeometryMapping &map)
        {
            const auto &prims=src.primitives;
            dst.subMeshes.reserve(prims.size());

            for(int32_t i=0; i<static_cast<int32_t>(prims.size()); ++i)
            {
                const auto &p=prims[static_cast<size_t>(i)];
                SubMesh sm;
                sm.geometry=map.geomIndexOfPrim[static_cast<size_t>(i)];
                sm.material=p.material?std::optional<int32_t>(static_cast<int32_t>(*p.material)):std::optional<int32_t>{};
                dst.subMeshes.push_back(std::move(sm));
            }
        }

        static void AttachNodeSubMeshes(Model &dst,const GLTFModel &src)
        {
            for(int32_t ni=0; ni<static_cast<int32_t>(src.nodes.size()); ++ni)
            {
                const auto &node=src.nodes[static_cast<size_t>(ni)];
                auto &pn=dst.mesh_nodes[static_cast<size_t>(ni)];
                if(node.mesh)
                {
                    const auto &mesh=src.meshes[static_cast<size_t>(*node.mesh)];
                    pn.subMeshes.reserve(mesh.primitives.size());
                    for(auto primIndex:mesh.primitives) pn.subMeshes.push_back(static_cast<int32_t>(primIndex));
                }
            }
        }

    } // anonymous namespace

    Model ConvertFromGLTF(const GLTFModel &src)
    {
        Model dst;
        dst.gltf_source=src.source;

        CopyMaterials(dst,src);
        CopyScenes(dst,src);
        CopyMeshNodesAndTransforms(dst,src);

        auto uniqueMap=BuildUniqueGeometryMapping(src);
        CreateUniqueGeometryEntries(dst,src,uniqueMap);
        BuildSubMeshes(dst,src,uniqueMap);
        AttachNodeSubMeshes(dst,src);

        return dst;
    }
} // namespace pure
