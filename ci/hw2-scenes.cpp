#include "rasterizer.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv) {
    if(argc!=5)return 2;
    int w=std::stoi(argv[1]),h=std::stoi(argv[2]),samples=std::stoi(argv[3]);
    rst::rasterizer r(w,h,samples);
    int count;std::cin>>count;
    std::vector<Eigen::Vector3f> positions,colors;std::vector<Eigen::Vector3i> indices;
    for(int t=0;t<count;++t) {
        Eigen::Vector3f color;std::cin>>color.x()>>color.y()>>color.z();
        for(int i=0;i<3;++i) {
            float x,y,z;std::cin>>x>>y>>z;
            positions.emplace_back(2*x/w-1,2*y/h-1,z);colors.push_back(color);
        }
        indices.emplace_back(3*t,3*t+1,3*t+2);
    }
    if(!std::cin)return 3;
    r.draw(r.load_positions(positions),r.load_indices(indices),r.load_colors(colors),rst::Primitive::Triangle);
    std::ofstream out(argv[4],std::ios::binary);
    for(auto color:r.frame_buffer())out.write(reinterpret_cast<const char*>(color.data()),3*sizeof(float));
    return out?0:4;
}
