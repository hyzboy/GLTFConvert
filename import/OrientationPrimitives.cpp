#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "gltf/Primitive.h"
#include "common/VkFormat.h"

namespace importers
{
    namespace
    {
        inline void RotateYUpToZUp(float &x,float &y,float &z)  { float  nx=x; float  ny=-z; float  nz=y; x=nx; y=ny; z=nz; }
        inline void RotateYUpToZUp(double &x,double &y,double &z){ double nx=x; double ny=-z; double nz=y; x=nx; y=ny; z=nz; }

        template<typename T>
        inline void RotateTripletArrayYUpToZUp(T* data, std::size_t count, std::size_t stride)
        {
            // stride: number of scalars per element (3 for POSITION/NORMAL/BITANGENT, 4 for TANGENT)
            for (std::size_t i = 0; i < count; ++i)
            {
                T &x = data[i * stride + 0];
                T &y = data[i * stride + 1];
                T &z = data[i * stride + 2];
                RotateYUpToZUp(x, y, z);
            }
        }
    }

    void RotatePrimitivesYUpToZUp(std::vector<GLTFPrimitive> &primitives)
    {
        glm::dmat4 rot(1.0);
        rot = glm::rotate(rot, glm::radians(90.0), glm::dvec3(1.0,0.0,0.0));
        for (auto &prim : primitives)
        {
            auto &geom = prim.geometry;
            for (auto &attr : geom.attributes)
            {
                const std::string &name = attr.name;
                if (name == "POSITION" || name == "NORMAL" || name == "TANGENT" || name == "BITANGENT")
                {
                    if (attr.data.empty() || attr.count == 0) continue;

                    bool isTangent = (name == "TANGENT");
                    std::size_t stride = isTangent ? 4 : 3; // we only rotate first 3 components

                    switch (attr.format)
                    {
                        case PF_RGBA32F: // tangent float4
                        case PF_RGB32F:  // vec3 float
                        {
                            if ((isTangent && attr.format == PF_RGBA32F) || (!isTangent && attr.format == PF_RGB32F))
                            {
                                auto *f = reinterpret_cast<float*>(attr.data.data());
                                RotateTripletArrayYUpToZUp<float>(f, attr.count, stride);
                            }
                            break;
                        }
                        case PF_RGBA64F: // tangent double4
                        case PF_RGB64F:  // vec3 double
                        {
                            if ((isTangent && attr.format == PF_RGBA64F) || (!isTangent && attr.format == PF_RGB64F))
                            {
                                auto *d = reinterpret_cast<double*>(attr.data.data());
                                RotateTripletArrayYUpToZUp<double>(d, attr.count, stride);
                            }
                            break;
                        }
                        default: break; // other formats not handled
                    }
                }
            }
            if (!geom.localAABB.empty())
            {
                geom.localAABB = geom.localAABB.transformed(rot);
            }
        }
    }
} // namespace importers
