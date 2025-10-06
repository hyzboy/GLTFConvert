#include "Orientation.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "common/VkFormat.h"

namespace importers {
namespace {
    inline void RotateYUpToZUp(float& x,float& y,float& z){ float nx=x; float ny=-z; float nz=y; x=nx; y=ny; z=nz; }
    inline void RotateYUpToZUp(double& x,double& y,double& z){ double nx=x; double ny=-z; double nz=y; x=nx; y=ny; z=nz; }
}

void RotatePrimitivesYUpToZUp(std::vector<GLTFPrimitive>& primitives)
{
    glm::dmat4 rot(1.0);
    rot = glm::rotate(rot, glm::radians(90.0), glm::dvec3(1.0,0.0,0.0));
    for (auto& prim : primitives) {
        auto& geom = prim.geometry;
        for (auto& attr : geom.attributes) {
            const std::string& name = attr.name;
            if (name=="POSITION"||name=="NORMAL"||name=="TANGENT"||name=="BITANGENT") {
                if (attr.data.empty()||attr.count==0) continue;
                if (name=="TANGENT") {
                    if (attr.format==PF_RGBA32F) {
                        float* f=reinterpret_cast<float*>(attr.data.data());
                        for (std::size_t i=0;i<attr.count;++i) { float &x=f[i*4+0]; float &y=f[i*4+1]; float &z=f[i*4+2]; RotateYUpToZUp(x,y,z); }
                    } else if (attr.format==PF_RGBA64F) {
                        double* d=reinterpret_cast<double*>(attr.data.data());
                        for (std::size_t i=0;i<attr.count;++i) { double &x=d[i*4+0]; double &y=d[i*4+1]; double &z=d[i*4+2]; RotateYUpToZUp(x,y,z); }
                    }
                } else if (name=="POSITION"||name=="NORMAL"||name=="BITANGENT") {
                    if (attr.format==PF_RGB32F) {
                        float* f=reinterpret_cast<float*>(attr.data.data());
                        for (std::size_t i=0;i<attr.count;++i) { float &x=f[i*3+0]; float &y=f[i*3+1]; float &z=f[i*3+2]; RotateYUpToZUp(x,y,z); }
                    } else if (attr.format==PF_RGB64F) {
                        double* d=reinterpret_cast<double*>(attr.data.data());
                        for (std::size_t i=0;i<attr.count;++i) { double &x=d[i*3+0]; double &y=d[i*3+1]; double &z=d[i*3+2]; RotateYUpToZUp(x,y,z); }
                    }
                }
            }
        }
        if(!geom.localAABB.empty()) {
            geom.localAABB = geom.localAABB.transformed(rot);
        }
    }
}

} // namespace importers
