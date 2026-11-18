#include "CGL/CGL.h"
#include "CGL/viewer.h"
#include "application.h"
#include <cmath>
#include <stdexcept>
#include <string>
#include <iostream>
int main(int argc,char** argv) {
    CGL::AppConfig config;
    try {
        auto number=[](const std::string& text) {
            size_t used;double value=std::stod(text,&used);
            if(used!=text.size()||!std::isfinite(value)) throw std::invalid_argument("Invalid number");
            return value;
        };
        for(int i=1;i<argc;++i) {
            std::string key=argv[i];
            if(i+1>=argc) throw std::invalid_argument("Missing option value");
            double value=number(argv[++i]);
            if(key=="-f") {
                if(value<1||value>100000||std::floor(value)!=value) throw std::invalid_argument("Invalid frame limit");
                config.frame_limit=int(value);
            } else if(key=="-s") {
                if(value<1||value>4096||std::floor(value)!=value) throw std::invalid_argument("Invalid steps");
                config.steps_per_frame=int(value);
            } else if(key=="-m") {
                if(value<=0||value>1e6) throw std::invalid_argument("Invalid mass");
                config.mass=float(value);
            } else if(key=="-g") {
                if(i+1>=argc) throw std::invalid_argument("Gravity needs x and y");
                config.gravity=CGL::Vector2D(value,number(argv[++i]));
            } else throw std::invalid_argument("Unknown option");
        }
        std::cout<<"START steps="<<config.steps_per_frame<<" nodes=16"<<std::endl;
        CGL::Viewer viewer;
        viewer.set_renderer(new CGL::Application(config));
        viewer.init();viewer.start();
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<"\nusage: ropesim [-s 1..4096] [-m positive] [-g x y] [-f freeze_after_frames]\n";
        return 1;
    }
}
