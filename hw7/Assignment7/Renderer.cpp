#include "Renderer.hpp"
#include <atomic>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <thread>

const float EPSILON=0.00001f;
void Renderer::Render(const Scene& scene,int spp,int threads,uint64_t seed,const std::string& output) {
    std::vector<Vector3f> framebuffer(scene.width*scene.height);
    const float scale=std::tan(float(scene.fov)*0.5f*M_PI/180);
    const float aspect=float(scene.width)/scene.height;
    const Vector3f eye(278,273,-800);
    std::atomic<int> nextRow{0},active{0},peak{0};
    auto worker=[&]() {
        int current=active.fetch_add(1)+1, previous=peak.load();
        while(current>previous&&!peak.compare_exchange_weak(previous,current)) {}
        for(int y;(y=nextRow.fetch_add(1))<scene.height;) {
            for(int x=0;x<scene.width;++x) {
                size_t pixel=size_t(y)*scene.width+x;
                seed_random(seed+UINT64_C(0x9e3779b97f4a7c15)*(pixel+1));
                float xx=(2*(x+0.5f)/scene.width-1)*aspect*scale;
                float yy=(1-2*(y+0.5f)/scene.height)*scale;
                Vector3f direction=normalize(Vector3f(-xx,yy,1));
                Vector3f color(0);
                for(int sample=0;sample<spp;++sample)
                    color+=scene.castRay(Ray(eye,direction),0)/float(spp);
                framebuffer[pixel]=color;
            }
        }
        active.fetch_sub(1);
    };
    auto start=std::chrono::steady_clock::now();
    if(threads==1) worker();
    else {
        std::vector<std::thread> workers;
        for(int i=0;i<threads;++i) workers.emplace_back(worker);
        for(auto& thread:workers) thread.join();
    }
    auto end=std::chrono::steady_clock::now();
    std::ofstream ppm(output,std::ios::binary);
    if(!ppm) throw std::runtime_error("Cannot write "+output);
    ppm<<"P6\n"<<scene.width<<' '<<scene.height<<"\n255\n";
    // Retain linear HDR samples for numerical comparisons before display clamp.
    std::ofstream hdr(output+".linear",std::ios::binary);
    if(!hdr) throw std::runtime_error("Cannot write linear image");
    for(const auto& color:framebuffer) {
        for(int axis=0;axis<3;++axis) {
            float value=float(color[axis]);
            if(!std::isfinite(value)||value<0) throw std::runtime_error("Invalid radiance");
            hdr.write(reinterpret_cast<const char*>(&value),sizeof(value));
            unsigned char channel=static_cast<unsigned char>(255*std::pow(clamp(0,1,value),0.6f));
            ppm.write(reinterpret_cast<const char*>(&channel),1);
        }
    }
    if(!ppm||!hdr) throw std::runtime_error("Image write failed");
    std::cout<<std::setprecision(12)<<"RENDER width="<<scene.width<<" height="<<scene.height
             <<" spp="<<spp<<" threads="<<threads<<" peak_workers="<<peak.load()<<" seed="<<seed
             <<" render_ms="<<std::chrono::duration<double,std::milli>(end-start).count()<<"\n";
}
