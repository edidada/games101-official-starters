#pragma once
#include "Vector.hpp"
#include "global.hpp"

enum MaterialType { DIFFUSE, MICROFACET };
class Material {
public:
    MaterialType m_type;
    Vector3f m_emission;
    float ior = 1.5f;
    Vector3f Kd = Vector3f(0.7f), Ks = Vector3f(0.04f);
    float specularExponent = 0;
    float roughness = 0.35f;
    Material(MaterialType type=DIFFUSE, Vector3f emission=Vector3f(0))
        : m_type(type), m_emission(emission) {}
    MaterialType getType() { return m_type; }
    Vector3f getEmission() { return m_emission; }
    bool hasEmission() { return m_emission.x>0 || m_emission.y>0 || m_emission.z>0; }
    Vector3f getColorAt(double, double) { return Kd; }

    // wi points into the surface, wo points away. Both types use the same
    // uniform-hemisphere distribution as allowed by the assignment.
    Vector3f sample(const Vector3f&, const Vector3f& N) {
        float z=get_random_float(), phi=2*M_PI*get_random_float();
        float radius=std::sqrt(std::max(0.f,1-z*z));
        Vector3f tangent=normalize(std::abs(N.x)>0.9f ? crossProduct(Vector3f(0,1,0),N)
                                                     : crossProduct(Vector3f(1,0,0),N));
        Vector3f bitangent=crossProduct(N,tangent);
        return tangent*(radius*std::cos(phi))+bitangent*(radius*std::sin(phi))+N*z;
    }
    float pdf(const Vector3f& wi, const Vector3f& wo, const Vector3f& N) {
        return dotProduct(-wi,N)>0 && dotProduct(wo,N)>0 ? 1/(2*M_PI) : 0;
    }
    Vector3f eval(const Vector3f& wi, const Vector3f& wo, const Vector3f& N) {
        double nv=dotProduct(-wi,N), nl=dotProduct(wo,N);
        if (!(nv>0 && nl>0)) return Vector3f(0);
        if (m_type==DIFFUSE) return Kd/M_PI;
        Vector3f H=normalize(wo-wi);
        double nh=std::max(0.f,dotProduct(N,H));
        double vh=std::max(0.f,dotProduct(-wi,H));
        double r=std::max(0.01f,std::min(1.f,roughness));
        double alpha=r*r, a2=alpha*alpha;
        double denominator=nh*nh*(a2-1)+1;
        double D=a2/(double(M_PI)*denominator*denominator);
        auto G1=[a2](double n) { return 2*n/(n+std::sqrt(a2+(1-a2)*n*n)); };
        Vector3f F=Ks+(Vector3f(1)-Ks)*float(std::pow(1-vh,5));
        Vector3f specular=F*float(D*G1(nv)*G1(nl)/(4*nv*nl));
        Vector3f diffuse=(Vector3f(1)-F)*Kd/M_PI;
        return diffuse+specular;
    }
};
