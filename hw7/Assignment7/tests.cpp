#include "Triangle.hpp"
#include "Sphere.hpp"
#include "Scene.hpp"
#include <iomanip>
#include <stdexcept>

const float EPSILON=0.00001f;
namespace {
void require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
void close(double a,double b,double tolerance,const char* message) { require(std::abs(a-b)<=tolerance,message); }
void randomStreams() {
    std::vector<float> first,second;
    for(uint64_t pixel=0;pixel<4096;++pixel) {
        seed_random(7+UINT64_C(0x9e3779b97f4a7c15)*(pixel+1));
        first.push_back(get_random_float());second.push_back(get_random_float());
    }
    int overlap=0;
    for(size_t i=1;i<first.size();++i) if(first[i]==second[i-1]) ++overlap;
    require(overlap<2,"adjacent pixel RNG streams overlap");
    double sumX=0,sumY=0,sumXX=0,sumYY=0,sumXY=0;
    for(size_t i=1;i<first.size();++i) {
        double x=first[i-1],y=first[i];
        sumX+=x;sumY+=y;sumXX+=x*x;sumYY+=y*y;sumXY+=x*y;
    }
    double n=first.size()-1;
    double correlation=(n*sumXY-sumX*sumY)/std::sqrt((n*sumXX-sumX*sumX)*(n*sumYY-sumY*sumY));
    require(std::abs(correlation)<0.08,"adjacent pixel RNG correlation");
    seed_random(123);std::vector<float> repeated;
    for(int i=0;i<64;++i) repeated.push_back(get_random_float());
    seed_random(123);
    for(float value:repeated) require(value==get_random_float(),"RNG seed reproducibility");
    std::cout<<"RNG pixels=4096 stream_shift_matches="<<overlap<<" neighbor_correlation="<<correlation<<" PASS\n";
}
void sampling() {
    seed_random(61);
    Material a(DIFFUSE,Vector3f(2)),b(DIFFUSE,Vector3f(3));
    Triangle small(Vector3f(0,0,2),Vector3f(2,0,2),Vector3f(0,2,2),&a);
    Triangle large(Vector3f(10,0,2),Vector3f(16,0,2),Vector3f(10,2,2),&b);
    Scene scene(1,1);scene.Add(&small);scene.Add(&large);scene.buildBVH();
    for(bool viaScene:{false,true}) {
        int chosenSmall=0;Vector3f sum(0);const int count=50000;
        for(int i=0;i<count;++i) {
            Intersection point;float pdf=0;
            if(viaScene) scene.sampleLight(point,pdf);else scene.bvh->Sample(point,pdf);
            close(pdf,1.0/8,1e-7,"area mixture PDF");
            require(point.m&&point.obj&&point.happened,"sample payload missing");
            if(point.obj==&small) { ++chosenSmall;sum+=point.coords; }
            require(point.emit.x==(point.obj==&small?2:3),"sample emission wrong");
        }
        close(double(chosenSmall)/count,0.25,0.009,"area selection frequency biased");
        close(sum.x/chosenSmall,2.0/3,0.02,"triangle uniform centroid x");
        close(sum.y/chosenSmall,2.0/3,0.02,"triangle uniform centroid y");
    }
    Material diffuse;diffuse.Kd=Vector3f(0.7f);
    Vector3f N=normalize(Vector3f(1,2,3)),wi=-N;double z=0,z2=0;
    for(int i=0;i<50000;++i) {
        Vector3f d=diffuse.sample(wi,N);
        close(dotProduct(d,d),1,1e-5,"hemisphere direction length");
        double cosine=dotProduct(d,N);require(cosine>=-1e-6,"hemisphere escaped");
        close(diffuse.pdf(wi,d,N),1/(2*double(M_PI)),1e-7,"hemisphere PDF");
        z+=cosine;z2+=cosine*cosine;
    }
    close(z/50000,0.5,0.008,"hemisphere mean");close(z2/50000,1.0/3,0.008,"hemisphere second moment");
    Sphere sphere(Vector3f(0),2,&a);double meanZ=0,meanZ2=0;
    for(int i=0;i<50000;++i) {
        Intersection point;float pdf;sphere.Sample(point,pdf);
        close(dotProduct(point.coords,point.coords),4,1e-5,"sphere point outside surface");
        close(pdf,1/(16*double(M_PI)),1e-7,"sphere PDF");
        meanZ+=point.normal.z;meanZ2+=point.normal.z*point.normal.z;
    }
    close(meanZ/50000,0,0.008,"sphere mean z");close(meanZ2/50000,1.0/3,0.008,"sphere second moment");
    std::cout<<"SAMPLING area_cdf=100000 hemisphere=50000 sphere=50000 PASS\n";
}
void materials() {
    Material metal(MICROFACET);metal.Kd=Vector3f(0);metal.Ks=Vector3f(0.85f,0.6f,0.2f);
    Vector3f N(0,0,1);seed_random(71);
    for(int i=0;i<2000;++i) {
        Vector3f v=metal.sample(Vector3f(0,0,-1),N),l=metal.sample(Vector3f(0,0,-1),N);
        metal.roughness=0.2f+0.8f*get_random_float();
        Vector3f f=metal.eval(-v,l,N),reverse=metal.eval(-l,v,N);
        for(int axis=0;axis<3;++axis) {
            require(std::isfinite(f[axis])&&f[axis]>=0,"invalid GGX BRDF");
            close(f[axis],reverse[axis],1e-5*std::max(1.0,double(f[axis])),"GGX reciprocity");
        }
    }
    // At normal incidence D=1/(pi*alpha^2), G=1, Schlick F=F0.
    for(float roughness:{0.2f,0.35f,0.7f,1.f}) {
        metal.roughness=roughness;
        Vector3f f=metal.eval(Vector3f(0,0,-1),N,N);
        double alpha=double(roughness)*roughness;
        close(f.x,0.85/(4*double(M_PI)*alpha*alpha),1e-5*std::max(1.f,f.x),"GGX normal analytic peak");
    }
    require(metal.eval(N,N,N).x==0&&metal.pdf(N,N,N)==0,"incoming hemisphere guard");
    require(metal.eval(-N,-N,N).x==0&&metal.pdf(-N,-N,N)==0,"outgoing hemisphere guard");
    // Independent midpoint solid-angle quadrature of directional albedo.
    for(float roughness:{0.35f,0.6f,1.f}) for(float viewZ:{0.2f,0.6f,1.f}) {
        metal.roughness=roughness;Vector3f v(std::sqrt(1-viewZ*viewZ),0,viewZ);
        double integral=0;
        const int nz=256,np=256;
        for(int iz=0;iz<nz;++iz) for(int ip=0;ip<np;++ip) {
            float z=(iz+0.5f)/nz,phi=2*M_PI*(ip+0.5f)/np;
            Vector3f l(std::sqrt(1-z*z)*std::cos(phi),std::sqrt(1-z*z)*std::sin(phi),z);
            integral+=metal.eval(-v,l,N).x*z*(2*double(M_PI)/(nz*np));
        }
        require(integral>0.1&&integral<=1.015,"GGX directional albedo outside physical bounds");
        std::cout<<"ALBEDO roughness="<<roughness<<" view_z="<<viewZ<<" value="<<integral<<'\n';
    }
    std::cout<<"MATERIAL reciprocity=2000 normal_peaks=4 energy_quadratures=9 PASS\n";
}
void transport() {
    Material receiver,emitter(DIFFUSE,Vector3f(10)),dark;
    receiver.Kd=Vector3f(0.6f);dark.Kd=Vector3f(0.5f);
    Triangle floor(Vector3f(-10,-10,0),Vector3f(10,-10,0),Vector3f(0,20,0),&receiver);
    Triangle a(Vector3f(-1,-1,2),Vector3f(-1,1,2),Vector3f(1,-1,2),&emitter);
    Triangle b(Vector3f(1,1,2),Vector3f(1,-1,2),Vector3f(-1,1,2),&emitter);
    Scene scene(1,1);scene.Add(&floor);scene.Add(&a);scene.Add(&b);scene.buildBVH();
    // Independent square-emitter irradiance integral by midpoint quadrature.
    double reference=0;const int grid=500;
    for(int y=0;y<grid;++y) for(int x=0;x<grid;++x) {
        double xx=-1+2*(x+0.5)/grid,yy=-1+2*(y+0.5)/grid;
        double r2=xx*xx+yy*yy+4;
        reference+=10*0.6/double(M_PI)*4/(r2*r2)*4/(grid*grid);
    }
    for(float roulette:{0.f,0.8f}) {
        scene.RussianRoulette=roulette;seed_random(108+int(roulette*10));
        double sum=0,squared=0;const int count=50000;
        for(int i=0;i<count;++i) {
            Vector3f value=scene.castRay(Ray(Vector3f(0,0,0.5f),Vector3f(0,0,-1)),0);
            close(value.x,value.y,1e-6,"neutral channel mismatch");
            sum+=value.x;squared+=value.x*value.x;
        }
        double mean=sum/count,variance=std::max(0.,squared/count-mean*mean);
        double se=std::sqrt(variance/count);
        close(mean,reference,6*se+0.0005,"direct transport quadrature disagrees");
        std::cout<<"DIRECT roulette="<<roulette<<" mean="<<mean<<" reference="<<reference<<" standard_error="<<se<<'\n';
    }
    close(scene.castRay(Ray(Vector3f(0,0,1),Vector3f(0,0,1)),0).x,10,1e-6,"camera emitter must be visible");
    close(scene.castRay(Ray(Vector3f(0,0,1),Vector3f(0,0,1)),1).x,0,0,"emitter double counted after NEE");
    close(scene.castRay(Ray(Vector3f(20,20,1),Vector3f(1,0,0)),0).x,0,0,"miss should be black");
    Triangle blockA(Vector3f(-10,-10,1),Vector3f(-10,10,1),Vector3f(10,-10,1),&dark);
    Triangle blockB(Vector3f(10,10,1),Vector3f(10,-10,1),Vector3f(-10,10,1),&dark);
    scene.Add(&blockA);scene.Add(&blockB);scene.buildBVH();scene.RussianRoulette=0;
    for(int i=0;i<1000;++i)
        close(scene.castRay(Ray(Vector3f(0,0,0.5f),Vector3f(0,0,-1)),0).x,0,0,"shadow segment leaked through blocker");
    Scene noLight(1,1);noLight.Add(&floor);noLight.buildBVH();
    Intersection point;float pdf=17;noLight.sampleLight(point,pdf);
    require(pdf==0&&!point.happened,"empty emitter set");
    close(noLight.castRay(Ray(Vector3f(0,0,0.5f),Vector3f(0,0,-1)),0).x,0,0,"unlit surface should be black");
    std::cout<<"TRANSPORT quadrature_samples=100000 blocked=1000 camera_emission=pass no_double_count=pass PASS\n";
}
}
int main(int argc,char** argv) {
    try { std::cout<<std::setprecision(12);randomStreams();
        if(argc==2&&std::string(argv[1])=="--rng-only") return 0;
        sampling();materials();transport(); }
    catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
