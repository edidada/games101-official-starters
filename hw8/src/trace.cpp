#include "rope.h"
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <iostream>
using namespace CGL;
int main(int argc,char** argv) {
    if(argc!=2) return 1;
    try {
        std::ofstream out(argv[1]);if(!out) throw std::runtime_error("Cannot write trace");
        out<<"method,time,node,x,y,pinned\n"<<std::setprecision(15);
        const int rate=4096;
        for(int method=0;method<4;++method) {
            Rope rope(Vector2D(0,200),Vector2D(-400,200),16,1,100,{0});
            const char* names[]={"explicit","semi","verlet","constraints"};
            for(int step=0;step<=15*rate;++step) {
                if(step==0||step==rate||step==2*rate||step==4*rate||step==8*rate||step==15*rate)
                    for(size_t i=0;i<rope.masses.size();++i) {
                        auto m=rope.masses[i];
                        if(!std::isfinite(m->position.x)||!std::isfinite(m->position.y)) throw std::runtime_error("Nonfinite trace");
                        out<<names[method]<<','<<double(step)/rate<<','<<i<<','<<m->position.x<<','<<m->position.y<<','<<m->pinned<<'\n';
                    }
                if(step==15*rate) break;
                if(method==3) rope.simulateConstraints(1.f/rate,Vector2D(0,-9.8),32);
                else if(method==2) rope.simulateVerlet(1.f/rate,Vector2D(0,-9.8));
                else rope.simulateEuler(1.f/rate,Vector2D(0,-9.8),method==0?Rope::EulerMethod::Explicit:Rope::EulerMethod::SemiImplicit);
            }
        }
        if(!out) throw std::runtime_error("Trace write failed");
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
