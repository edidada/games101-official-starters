#include "Renderer.hpp"
#include "Triangle.hpp"
#include "Sphere.hpp"
#include <stdexcept>
#include <string>

int main(int argc,char** argv) {
    try {
        int size=512,spp=16,threads=4; uint64_t seed=7;
        std::string output="binary.ppm",type="diffuse";
        float roughness=0.35f,roulette=0.8f;
        for(int i=1;i<argc;i+=2) {
            if(i+1>=argc) throw std::invalid_argument("Options require a value");
            std::string key=argv[i],value=argv[i+1]; size_t used=0;
            if(key=="--output") output=value;
            else if(key=="--material") type=value;
            else if(key=="--rr") { roulette=std::stof(value,&used); if(used!=value.size()) throw std::invalid_argument("roulette"); }
            else if(key=="--roughness") { roughness=std::stof(value,&used); if(used!=value.size()) throw std::invalid_argument("roughness"); }
            else if(key=="--seed") { seed=std::stoull(value,&used); if(used!=value.size()||value[0]=='-') throw std::invalid_argument("seed"); }
            else {
                int number=std::stoi(value,&used);
                if(used!=value.size()) throw std::invalid_argument("integer option");
                if(key=="--size") size=number;
                else if(key=="--spp") spp=number;
                else if(key=="--threads") threads=number;
                else throw std::invalid_argument("Unknown option "+key);
            }
        }
        if(size<1||size>2048||spp<1||spp>4096||threads<1||threads>64||
           !std::isfinite(roughness)||roughness<0.08f||roughness>1||
           !std::isfinite(roulette)||roulette<0||roulette>0.95f||
           (type!="diffuse"&&type!="metal")) throw std::invalid_argument("Invalid render settings");
        Scene scene(size,size); scene.RussianRoulette=roulette;
        Material red,green,white;
        red.Kd=Vector3f(0.63f,0.065f,0.05f);
        green.Kd=Vector3f(0.14f,0.45f,0.091f);
        white.Kd=Vector3f(0.725f,0.71f,0.68f);
        Material light(DIFFUSE,8.0f*Vector3f(0.805f,1.005f,0.747f)+
            15.6f*Vector3f(1.027f,0.900f,0.740f)+18.4f*Vector3f(1.379f,0.896f,0.737f));
        Material metal(MICROFACET);metal.Kd=Vector3f(0);
        metal.Ks=Vector3f(0.85f,0.6f,0.2f);metal.roughness=roughness;
        MeshTriangle floor("../models/cornellbox/floor.obj",&white);
        MeshTriangle shortbox("../models/cornellbox/shortbox.obj",&white);
        MeshTriangle tallbox("../models/cornellbox/tallbox.obj",&white);
        MeshTriangle left("../models/cornellbox/left.obj",&red);
        MeshTriangle right("../models/cornellbox/right.obj",&green);
        MeshTriangle ceilingLight("../models/cornellbox/light.obj",&light);
        for(Object* object:{&floor,&shortbox,&tallbox,&left,&right,&ceilingLight}) scene.Add(object);
        Sphere sphere(Vector3f(380,80,100),80,&metal);
        if(type=="metal") scene.Add(&sphere);
        scene.buildBVH();
        std::cout<<"MATERIAL "<<type<<" roughness="<<roughness<<" roulette="<<roulette<<"\n";
        Renderer().Render(scene,spp,threads,seed,output);
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<"\nusage: RayTracing [--size N] [--spp N] [--threads N] [--seed N] "
                   "[--material diffuse|metal] [--roughness 0.08..1] [--output file.ppm]\n";
        return 1;
    }
}
