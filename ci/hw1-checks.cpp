#include <eigen3/Eigen/Eigen>
#include "rasterizer.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
Eigen::Matrix4f get_rotation(Eigen::Vector3f,float);
Eigen::Matrix4f get_model_matrix(float);
Eigen::Matrix4f get_view_matrix(Eigen::Vector3f);
Eigen::Matrix4f get_projection_matrix(float,float,float,float);
int main() {
    using namespace Eigen;
    assert((get_model_matrix(90)*Vector4f(2,0,-2,1)-Vector4f(0,2,-2,1)).norm()<1e-5);
    assert((get_model_matrix(-90)*Vector4f(2,0,-2,1)-Vector4f(0,-2,-2,1)).norm()<1e-5);
    assert((get_view_matrix(Vector3f(0,0,5))*Vector4f(2,0,-2,1)-Vector4f(2,0,-7,1)).norm()<1e-5);
    for(int i=0;i<200;++i) {
        Vector3f axis(1+i%7,-2-i%11,3+i%13);
        float angle=-720+i*13.25f;
        Matrix4f r=get_rotation(axis,angle);
        assert((r.transpose()*r-Matrix4f::Identity()).norm()<2e-5);
        assert(std::abs(r.determinant()-1)<1e-5);
        assert((r*Vector4f(axis.x(),axis.y(),axis.z(),0)-Vector4f(axis.x(),axis.y(),axis.z(),0)).norm()<2e-5);
        assert((r*get_rotation(axis,-angle)-Matrix4f::Identity()).norm()<2e-5);
        assert((get_rotation(axis*7,angle)-r).norm()<1e-5);
        assert((get_rotation(axis,angle+360)-r).norm()<1e-5);
        Vector3f point(.3f,-.7f,1.2f),unit=axis.normalized();
        double radians=angle*std::acos(-1.0)/180;
        // Independent geometric decomposition into axial and perpendicular parts.
        Vector3f parallel=unit*unit.dot(point),perp=point-parallel;
        Vector3f expected=parallel+perp*std::cos(radians)+unit.cross(perp)*std::sin(radians);
        assert((r.block<3,3>(0,0)*point-expected).norm()<2e-5);
    }
    unsigned projections=0;
    for(float fov:{20.f,45.f,90.f,135.f})for(float aspect:{.5f,1.f,2.f})
    for(float near:{.1f,1.f})for(float far:{10.f,50.f}) {
        Matrix4f p=get_projection_matrix(fov,aspect,near,far);
        const double tangent=std::tan(fov*std::acos(-1.0)/360);
        for(float depth:{near,far}) {
            for(int x:{-1,1})for(int y:{-1,1}) {
                Vector4f v=p*Vector4f(x*depth*tangent*aspect,y*depth*tangent,-depth,1);
                v/=v.w();
                assert(std::abs(v.x()-x)<1e-5 && std::abs(v.y()-y)<1e-5);
                assert(std::abs(v.z()-(depth==near?-1:1))<1e-5);
            }
        }
        ++projections;
    }
    unsigned invalid=0;
    for(auto args:{Vector4f(0,1,.1,50),Vector4f(180,1,.1,50),Vector4f(45,0,.1,50),Vector4f(45,1,0,50),Vector4f(45,1,2,1),Vector4f(45,1,.1,std::numeric_limits<float>::infinity())}) {
        try {get_projection_matrix(args[0],args[1],args[2],args[3]);assert(false);}
        catch(const std::invalid_argument&){++invalid;}
    }
    try {get_rotation(Vector3f::Zero(),30);assert(false);}catch(const std::invalid_argument&){++invalid;}
    try {get_rotation(Vector3f::UnitX(),std::numeric_limits<float>::quiet_NaN());assert(false);}catch(const std::invalid_argument&){++invalid;}
    rst::rasterizer raster(4,3);raster.clear(rst::Buffers::Color);
    raster.set_pixel(Vector3f(0,0,0),Vector3f(1,2,3));
    raster.set_pixel(Vector3f(3,2,0),Vector3f(4,5,6));
    raster.set_pixel(Vector3f(-1,0,0),Vector3f(9,9,9));
    assert(raster.frame_buffer()[8]==Vector3f(1,2,3));
    assert(raster.frame_buffer()[3]==Vector3f(4,5,6));
    std::cout<<"PASS: 200 arbitrary-axis rotations, "<<projections<<" perspective frusta, "<<invalid<<" invalid inputs, z rotation/view and bottom/top framebuffer bounds\n";
}
