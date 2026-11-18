#pragma once

#include "BVH.hpp"
#include "Intersection.hpp"
#include "Material.hpp"
#include "OBJ_Loader.hpp"
#include "Object.hpp"
#include "Triangle.hpp"
#include <cassert>
#include <array>
#include <memory>
#include <stdexcept>

class Triangle : public Object
{
public:
    Vector3f v0, v1, v2; // vertices A, B ,C , counter-clockwise order
    Vector3f e1, e2;     // 2 edges v1-v0, v2-v0;
    Vector3f t0, t1, t2; // texture coords
    Vector3f normal;
    float area;
    Material* m;

    Triangle(Vector3f _v0, Vector3f _v1, Vector3f _v2, Material* _m = nullptr)
        : v0(_v0), v1(_v1), v2(_v2), m(_m)
    {
        e1 = v1 - v0;
        e2 = v2 - v0;
        normal = normalize(crossProduct(e1, e2));
        area = crossProduct(e1, e2).norm()*0.5f;
    }

    bool intersect(const Ray& ray) override;
    bool intersect(const Ray& ray, float& tnear,
                   uint32_t& index) const override;
    Intersection getIntersection(Ray ray) override;
    void getSurfaceProperties(const Vector3f& P, const Vector3f& I,
                              const uint32_t& index, const Vector2f& uv,
                              Vector3f& N, Vector2f& st) const override
    {
        N = normal;
        //        throw std::runtime_error("triangle::getSurfaceProperties not
        //        implemented.");
    }
    Vector3f evalDiffuseColor(const Vector2f&) const override;
    Bounds3 getBounds() override;
    void Sample(Intersection &pos, float &pdf){
        float x = std::sqrt(get_random_float()), y = get_random_float();
        pos.coords = v0 * (1.0f - x) + v1 * (x * (1.0f - y)) + v2 * (x * y);
        pos.normal = this->normal;
        pos.obj = this; pos.m = m; pos.emit = m ? m->getEmission() : Vector3f(0);
        pos.happened = area > 0;
        pdf = area > 0 ? 1.0f / area : 0;
    }
    float getArea(){
        return area;
    }
    bool hasEmit(){
        return m && m->hasEmission();
    }
};

class MeshTriangle : public Object
{
public:
    MeshTriangle(const std::string& filename, Material *mt)
    {
        objl::Loader loader;
        if (!loader.LoadFile(filename) || loader.LoadedMeshes.size()!=1)
            throw std::runtime_error("Expected one mesh in " + filename);
        area = 0;
        m = mt;
        assert(loader.LoadedMeshes.size() == 1);
        auto mesh = loader.LoadedMeshes[0];

        Vector3f min_vert = Vector3f{std::numeric_limits<float>::infinity(),
                                     std::numeric_limits<float>::infinity(),
                                     std::numeric_limits<float>::infinity()};
        Vector3f max_vert = Vector3f{-std::numeric_limits<float>::infinity(),
                                     -std::numeric_limits<float>::infinity(),
                                     -std::numeric_limits<float>::infinity()};
        for (int i = 0; i < mesh.Vertices.size(); i += 3) {
            std::array<Vector3f, 3> face_vertices;

            for (int j = 0; j < 3; j++) {
                auto vert = Vector3f(mesh.Vertices[i + j].Position.X,
                                     mesh.Vertices[i + j].Position.Y,
                                     mesh.Vertices[i + j].Position.Z);
                face_vertices[j] = vert;

                min_vert = Vector3f(std::min(min_vert.x, vert.x),
                                    std::min(min_vert.y, vert.y),
                                    std::min(min_vert.z, vert.z));
                max_vert = Vector3f(std::max(max_vert.x, vert.x),
                                    std::max(max_vert.y, vert.y),
                                    std::max(max_vert.z, vert.z));
            }

            triangles.emplace_back(face_vertices[0], face_vertices[1],
                                   face_vertices[2], mt);
        }

        bounding_box = Bounds3(min_vert, max_vert);

        std::vector<Object*> ptrs;
        for (auto& tri : triangles){
            ptrs.push_back(&tri);
            area += tri.area;
        }
        bvh = std::make_unique<BVHAccel>(ptrs);
    }

    bool intersect(const Ray& ray) { return bvh->IntersectP(ray); }
    bool intersect(const Ray& ray, float& tnear, uint32_t& index) const {
        auto hit=bvh->Intersect(ray);
        if (!hit.happened || hit.distance>=tnear) return false;
        tnear=float(hit.distance);
        index=uint32_t(static_cast<Triangle*>(hit.obj)-triangles.data());
        return true;
    }

    Bounds3 getBounds() { return bounding_box; }

    void getSurfaceProperties(const Vector3f& P, const Vector3f& I,
                              const uint32_t& index, const Vector2f& uv,
                              Vector3f& N, Vector2f& st) const
    {
        N = triangles.at(index).normal; st = uv;
    }

    Vector3f evalDiffuseColor(const Vector2f& st) const
    {
        float scale = 5;
        float pattern =
            (fmodf(st.x * scale, 1) > 0.5) ^ (fmodf(st.y * scale, 1) > 0.5);
        return lerp(Vector3f(0.815, 0.235, 0.031),
                    Vector3f(0.937, 0.937, 0.231), pattern);
    }

    Intersection getIntersection(Ray ray)
    {
        Intersection intersec;

        if (bvh) {
            intersec = bvh->Intersect(ray);
        }

        return intersec;
    }
    
    void Sample(Intersection &pos, float &pdf){
        bvh->Sample(pos, pdf);
        pos.emit = m->getEmission();
    }
    float getArea(){
        return area;
    }
    bool hasEmit(){
        return m && m->hasEmission();
    }

    Bounds3 bounding_box;
    std::vector<Triangle> triangles;
    std::unique_ptr<BVHAccel> bvh;
    float area;

    Material* m;
};

inline bool Triangle::intersect(const Ray& ray) { return getIntersection(ray).happened; }
inline bool Triangle::intersect(const Ray& ray, float& tnear,
                                uint32_t& index) const
{
    Intersection hit = const_cast<Triangle*>(this)->getIntersection(ray);
    if (!hit.happened || hit.distance >= tnear) return false;
    tnear = static_cast<float>(hit.distance);
    index = 0;
    return true;
}

inline Bounds3 Triangle::getBounds() { return Union(Bounds3(v0, v1), v2); }

inline Intersection Triangle::getIntersection(Ray ray)
{
    Intersection inter;
    using D = std::array<double, 3>;
    auto cross = [](const D& a, const D& b) -> D {
        return {a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]};
    };
    auto dot = [](const D& a, const D& b) {
        return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
    };
    D a{}, b{}, origin{}, direction{};
    for (int axis = 0; axis < 3; ++axis) {
        a[axis] = double(v1[axis]) - v0[axis];
        b[axis] = double(v2[axis]) - v0[axis];
        origin[axis] = double(ray.origin[axis]) - v0[axis];
        direction[axis] = ray.direction[axis];
    }
    D p = cross(direction, b);
    double det = dot(a, p);
    // Preserve the starter's front-face-only convention. No absolute epsilon:
    // a small triangle is not necessarily degenerate.
    if (!(det > 0) || !std::isfinite(det)) return inter;
    double u = dot(origin, p) / det;
    D q = cross(origin, a);
    double v = dot(direction, q) / det;
    if (!(u >= 0 && v >= 0 && u + v <= 1)) return inter;
    double t = dot(b, q) / det;
    if (!std::isfinite(t) || t < ray.t_min || t > ray.t_max) return inter;
    inter.happened = true;
    inter.distance = t;
    inter.coords = ray(t);
    inter.normal = normal;
    inter.obj = this;
    inter.m = m;
    inter.emit = m ? m->getEmission() : Vector3f(0);
    return inter;
}

inline Vector3f Triangle::evalDiffuseColor(const Vector2f&) const
{
    return Vector3f(0.5, 0.5, 0.5);
}
