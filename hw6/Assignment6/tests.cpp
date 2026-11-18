#include "Triangle.hpp"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <random>
#include <set>
#include <stdexcept>
#include <string>

const float EPSILON = 0.00001f;
namespace {
void require(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}
using D = std::array<long double, 3>;
D vec(const Vector3f& v) { return {v.x,v.y,v.z}; }
D sub(D a, D b) { for (int i=0;i<3;++i) a[i]-=b[i]; return a; }
D cross(D a,D b) { return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]}; }
long double dot(D a,D b) { return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; }
// Independent plane intersection followed by a Gram-matrix barycentric solve.
// This intentionally does not use Moller-Trumbore or the BVH/slab code.
long double triangleOracle(const Triangle& tri,const Ray& ray) {
    D a=vec(tri.v0), e=sub(vec(tri.v1),a), f=sub(vec(tri.v2),a);
    D n=cross(e,f), d=vec(ray.direction), o=vec(ray.origin);
    long double denominator=dot(n,d);
    if (!(denominator<0)) return -1;
    long double t=dot(n,sub(a,o))/denominator;
    if (t<ray.t_min || t>ray.t_max) return -1;
    D point{};
    for(int axis=0;axis<3;++axis) point[axis]=o[axis]+t*d[axis]-a[axis];
    long double ee=dot(e,e), ff=dot(f,f), ef=dot(e,f);
    long double det=ee*ff-ef*ef;
    if (!(det>0)) return -1;
    long double u=(dot(point,e)*ff-dot(point,f)*ef)/det;
    long double v=(dot(point,f)*ee-dot(point,e)*ef)/det;
    return u>=0 && v>=0 && u+v<=1 ? t : -1;
}
bool boxOracle(const Bounds3& box,const Ray& ray) {
    if (ray.t_min>ray.t_max) return false;
    for(int axis=0;axis<3;++axis) if(box.pMin[axis]>box.pMax[axis]) return false;
    auto inside=[&](long double t) {
        for(int a=0;a<3;++a) {
            long double v=static_cast<long double>(ray.origin[a])+t*ray.direction[a];
            if(v<box.pMin[a] || v>box.pMax[a]) return false;
        }
        return true;
    };
    if(inside(ray.t_min)) return true;
    // Enumerate face crossings, then check both other coordinates.
    for(int a=0;a<3;++a) if(ray.direction[a]!=0) for(int face=0;face<2;++face) {
        long double t=(static_cast<long double>(box[face][a])-ray.origin[a])/ray.direction[a];
        if(t<ray.t_min || t>ray.t_max) continue;
        bool ok=true;
        for(int b=0;b<3;++b) if(b!=a) {
            long double v=static_cast<long double>(ray.origin[b])+t*ray.direction[b];
            if(v<box.pMin[b] || v>box.pMax[b]) ok=false;
        }
        if(ok) return true;
    }
    return false;
}
void compareBox(const Bounds3& b, const Ray& r) {
    require(b.IntersectP(r,r.direction_inv,{})==boxOracle(b,r),"AABB face oracle mismatch");
}
size_t topology(BVHBuildNode* node,std::set<Object*>& leaves) {
    if(!node) return 0;
    if(node->object) {
        require(!node->left&&!node->right,"leaf has children");
        require(leaves.insert(node->object).second,"duplicate primitive");
        return 1;
    }
    require(node->left&&node->right,"incomplete internal node");
    for(auto child:{node->left,node->right}) for(int axis=0;axis<3;++axis)
        require(node->bounds.pMin[axis]<=child->bounds.pMin[axis] &&
                node->bounds.pMax[axis]>=child->bounds.pMax[axis],"parent bounds omit child");
    return topology(node->left,leaves)+topology(node->right,leaves);
}
void compareHit(const Intersection& hit,long double expected) {
    require(hit.happened==(expected>=0),"hit/miss mismatch");
    if(hit.happened) require(std::abs(hit.distance-expected)<=1e-7L*std::max(1.L,std::abs(expected)),"nearest distance mismatch");
}
void correctness() {
    std::mt19937 random(610106);
    std::uniform_real_distribution<float> value(-20,20);
    Bounds3 unit(Vector3f(-1),Vector3f(1));
    size_t boxes=0;
    for(int i=0;i<12000;++i) {
        Bounds3 b(Vector3f(value(random),value(random),value(random)),Vector3f(value(random),value(random),value(random)));
        Ray r(Vector3f(value(random),value(random),value(random)),Vector3f(value(random),value(random),value(random)));
        r.t_max=i%3 ? 100 : 0.25;
        if(i%5==0) r.direction=Vector3f(0,r.direction.y,0);
        r.direction_inv=Vector3f(1.f/r.direction.x,1.f/r.direction.y,1.f/r.direction.z);
        compareBox(b,r); ++boxes;
    }
    for(float x:{-2.f,-1.f,0.f,1.f,2.f}) for(float y:{-1.f,0.f,1.f})
        for(float d:{-1.f,-0.f,0.f,1.f}) {
            Ray r(Vector3f(x,y,0),Vector3f(d,0,0));
            compareBox(unit,r); ++boxes;
        }
    Ray forward(Vector3f(0,0,2),Vector3f(0,0,-1));
    for(double a:{0.,1.,2.,3.,4.}) for(double b:{0.,1.,2.,3.,4.}) {
        forward.t_min=a; forward.t_max=b; compareBox(unit,forward); ++boxes;
    }
    compareBox(Bounds3(),forward); ++boxes;
    Ray tiny(Vector3f(-2,0,0),Vector3f(std::numeric_limits<float>::denorm_min(),0,0));
    compareBox(unit,tiny); ++boxes;
    compareBox(Bounds3(Vector3f(0)),Ray(Vector3f(0,0,1),Vector3f(0,0,-1))); ++boxes;

    Material material;
    Triangle tri(Vector3f(0,0,0),Vector3f(2,0,0),Vector3f(0,2,0),&material);
    size_t triangleChecks=0;
    for(int i=0;i<1200;++i) {
        float u=0.01f+float(random()%900)/1000.f;
        float v=0.01f+float(random()%900)/1000.f;
        if(u+v>0.98f) { u*=0.4f; v*=0.4f; }
        Ray r(Vector3f(2*u,2*v,1+i%7),Vector3f(0,0,-1));
        auto hit=tri.getIntersection(r);
        compareHit(hit,triangleOracle(tri,r));
        require(hit.obj==&tri && hit.m==&material && std::abs(hit.coords.z)<1e-6,"intersection payload");
        r.t_max=0.5; compareHit(tri.getIntersection(r),-1);
        r.t_max=100; r.direction=Vector3f(0,0,1); compareHit(tri.getIntersection(r),-1);
        triangleChecks+=3;
    }
    for(auto r:{Ray(Vector3f(0,0,1),Vector3f(0,0,-1)),
                Ray(Vector3f(1,1,1),Vector3f(0,0,-1)),
                Ray(Vector3f(3,3,1),Vector3f(0,0,-1)),
                Ray(Vector3f(0.2,0.2,1),Vector3f(1,0,0)),
                Ray(Vector3f(0.2,0.2,-1),Vector3f(0,0,-1)),
                Ray(Vector3f(0.2,0.2,0),Vector3f(0,0,-1))}) {
        compareHit(tri.getIntersection(r),triangleOracle(tri,r)); ++triangleChecks;
    }
    Triangle small(Vector3f(0),Vector3f(1e-8f,0,0),Vector3f(0,1e-8f,0));
    Ray smallRay(Vector3f(2e-9f,2e-9f,1),Vector3f(0,0,-1));
    compareHit(small.getIntersection(smallRay),triangleOracle(small,smallRay)); ++triangleChecks;
    Triangle degenerate(Vector3f(0),Vector3f(1),Vector3f(2));
    require(!degenerate.getIntersection(forward).happened,"degenerate hit"); ++triangleChecks;

    // Arbitrarily oriented triangles over twelve orders of area: the plane/
    // Gram oracle must agree without sharing the production determinant code.
    for(int i=0;i<1000;++i) {
        float scale=i%3==0?1e-6f:(i%3==1?1.f:1e6f);
        Vector3f base(value(random)*scale,value(random)*scale,value(random)*scale);
        Vector3f edgeA(value(random)*scale,value(random)*scale,value(random)*scale);
        Vector3f edgeB(value(random)*scale,value(random)*scale,value(random)*scale);
        Triangle rotated(base,base+edgeA,base+edgeB,&material);
        Vector3f target=rotated.v0*0.5f+rotated.v1*0.2f+rotated.v2*0.3f;
        Ray r(target+rotated.normal*(30*scale),-rotated.normal);
        auto expected=triangleOracle(rotated,r);
        require(expected>=0,"constructed oriented reference missed");
        compareHit(rotated.getIntersection(r),expected);
        r.direction=-r.direction;
        compareHit(rotated.getIntersection(r),-1);
        triangleChecks+=2;
    }
    Ray boundary(Vector3f(0.5,0.5,1),Vector3f(0,0,-1));
    boundary.t_min=boundary.t_max=1;
    compareHit(tri.getIntersection(boundary),1); ++triangleChecks;
    float nearest=100; uint32_t index=99;
    require(tri.intersect(boundary,nearest,index)&&nearest==1&&index==0,"triangle interface");

    std::vector<Triangle> triangles;
    triangles.reserve(256);
    for(int i=0;i<256;++i) {
        Vector3f v(value(random),value(random),value(random));
        triangles.emplace_back(v,v+Vector3f(0.2f+std::abs(value(random)),0,0),
                               v+Vector3f(0,0.2f+std::abs(value(random)),0),&material);
    }
    std::vector<Object*> objects;
    for(auto& t:triangles) objects.push_back(&t);
    BVHAccel naive(objects), sah(objects,1,BVHAccel::SplitMethod::SAH), empty({});
    require(!empty.root&&!empty.Intersect(forward).happened,"empty tree");
    std::set<Object*> seen;
    require(topology(naive.root,seen)==256,"median leaf count"); seen.clear();
    require(topology(sah.root,seen)==256,"SAH leaf count");
    BVHAccel single({&tri});
    compareHit(single.Intersect(Ray(Vector3f(0.25,0.25,1),Vector3f(0,0,-1))),1);
    // Check the root SAH decision against an independently exhaustive split oracle.
    long double best=std::numeric_limits<long double>::infinity();
    for(int axis=0;axis<3;++axis) {
        auto sorted=objects;
        std::stable_sort(sorted.begin(),sorted.end(),[axis](Object* a,Object* b){
            return a->getBounds().Centroid()[axis]<b->getBounds().Centroid()[axis];
        });
        for(size_t split=1;split<sorted.size();++split) {
            Bounds3 l,r;
            for(size_t i=0;i<sorted.size();++i) {
                if(i<split) l=Union(l,sorted[i]->getBounds());
                else r=Union(r,sorted[i]->getBounds());
            }
            best=std::min(best,static_cast<long double>(split*l.SurfaceArea()+(sorted.size()-split)*r.SurfaceArea()));
        }
    }
    seen.clear(); size_t nl=topology(sah.root->left,seen); seen.clear(); size_t nr=topology(sah.root->right,seen);
    double chosen=nl*sah.root->left->bounds.SurfaceArea()+nr*sah.root->right->bounds.SurfaceArea();
    require(std::abs(chosen-best)<1e-6L*best,"SAH root is not minimum sweep cost");
    size_t rays=0;
    for(int i=0;i<3000;++i) {
        Ray r(Vector3f(value(random),value(random),30),Vector3f(value(random)*0.03f,value(random)*0.03f,-1));
        r.t_min=i%7==0?20:0; r.t_max=i%4==0?25:100;
        long double expected=-1;
        for(const auto& t:triangles) {
            auto d=triangleOracle(t,r);
            if(d>=0 && (expected<0||d<expected)) expected=d;
        }
        compareHit(naive.Intersect(r),expected); compareHit(sah.Intersect(r),expected);
        ++rays;
    }
    // Equal-centroid geometry must terminate and keep each object once.
    std::vector<Triangle> coincident(128,tri);
    objects.clear(); for(auto& t:coincident) objects.push_back(&t);
    BVHAccel ties(objects,1,BVHAccel::SplitMethod::SAH); seen.clear();
    require(topology(ties.root,seen)==128,"equal centroid tree");
    std::cout<<"CORRECTNESS boxes="<<boxes<<" triangle_checks="<<triangleChecks
             <<" nearest_rays="<<rays<<" independent_sah_root=pass topology=pass\n";
}
void benchmark() {
    MeshTriangle bunny("../models/bunny/bunny.obj");
    std::vector<Object*> objects;
    for(auto& t:bunny.triangles) objects.push_back(&t);
    std::vector<Ray> rays;
    for(int y=0;y<144;++y) for(int x=0;x<192;++x) {
        float xx=(2*(x+0.5f)/192-1)*192/144;
        float yy=1-2*(y+0.5f)/144;
        rays.emplace_back(Vector3f(-1,5,10),normalize(Vector3f(xx,yy,-1)));
    }
    std::vector<double> reference;
    for(auto& r:rays) {
        auto hit=bunny.bvh->Intersect(r);
        reference.push_back(hit.happened?hit.distance:-1);
    }
    bool checkedMeshInterface=false;
    for(auto& r:rays) {
        auto hit=bunny.getIntersection(r);
        if(!hit.happened) continue;
        float nearest=std::numeric_limits<float>::max(); uint32_t index=0;
        require(bunny.intersect(r)&&bunny.intersect(r,nearest,index),"mesh interface missed");
        require(index<bunny.triangles.size()&&std::abs(nearest-hit.distance)<1e-5,"mesh index/distance");
        checkedMeshInterface=true;
        break;
    }
    require(checkedMeshInterface,"bunny had no hits");
    size_t bruteChecks=0;
    for(size_t i=0;i<rays.size();i+=107) {
        long double expected=-1;
        for(auto& t:bunny.triangles) {
            long double d=triangleOracle(t,rays[i]);
            if(d>=0&&(expected<0||d<expected)) expected=d;
        }
        compareHit(bunny.bvh->Intersect(rays[i]),expected); ++bruteChecks;
    }
    std::ofstream csv("benchmark.csv");
    csv<<"round,method,triangles,rays,build_ms,trace_ms,hits,distance_sum\n"<<std::setprecision(15);
    using Clock=std::chrono::steady_clock;
    // Round zero warms both paths; three measured rounds alternate ordering.
    for(int round=0;round<4;++round) for(int order=0;order<2;++order) {
        bool useSAH=(round+order)%2;
        auto start=Clock::now();
        BVHAccel tree(objects,1,useSAH?BVHAccel::SplitMethod::SAH:BVHAccel::SplitMethod::NAIVE);
        auto built=Clock::now();
        size_t hits=0; double sum=0;
        for(size_t i=0;i<rays.size();++i) {
            auto hit=tree.Intersect(rays[i]);
            double d=hit.happened?hit.distance:-1;
            require(std::abs(d-reference[i])<1e-9,"benchmark median/SAH mismatch");
            if(hit.happened) { ++hits; sum+=d; }
        }
        auto traced=Clock::now();
        if(round) csv<<round<<','<<(useSAH?"sah":"naive")<<','<<objects.size()<<','<<rays.size()<<','
            <<std::chrono::duration<double,std::milli>(built-start).count()<<','
            <<std::chrono::duration<double,std::milli>(traced-built).count()<<','<<hits<<','<<sum<<'\n';
    }
    require(bool(csv),"benchmark output failed");
    std::cout<<"BENCHMARK rays="<<rays.size()<<" independent_bunny_rays="<<bruteChecks<<" measured_rounds=3\n";
}
}
int main(int argc,char** argv) {
    try {
        if(argc==2&&std::string(argv[1])=="--benchmark") benchmark();
        else if(argc==1) correctness();
        else throw std::invalid_argument("usage: hw6_tests [--benchmark]");
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n'; return 1;
    }
}
