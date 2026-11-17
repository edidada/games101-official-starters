#include "Triangle.hpp"
#include "Renderer.hpp"
#include <cassert>
#include <iostream>
int main() {
 unsigned hits=0;
 for(int i=0;i<1000;++i) {
  Vector3f a(-2+i%7,1+i%11,-5-i%13),e1(2,0,.2),e2(.1,3,.3);
  float u=.1f+(i%7)*.08f,v=.1f+(i%5)*.05f;
  Vector3f p=a+e1*u+e2*v,n=normalize(crossProduct(e1,e2));
  float t=99,uu=99,vv=99;
  assert(rayTriangleIntersect(a,a+e1,a+e2,p+n*4,-n,t,uu,vv));
  assert(std::abs(t-4)<1e-4 && std::abs(uu-u)<1e-4 && std::abs(vv-v)<1e-4);
  assert(rayTriangleIntersect(a,a+e2,a+e1,p-n*4,n,t,uu,vv));
  assert(std::abs(t-4)<1e-4 && std::abs(uu-v)<1e-4 && std::abs(vv-u)<1e-4);
  assert(!rayTriangleIntersect(a,a+e1,a+e2,p+n*4,n,t,uu,vv));
  assert(!rayTriangleIntersect(a,a+e1,a+e2,p+n*4,e1,t,uu,vv));
  assert(!rayTriangleIntersect(a,a+e1,a+e2,a+e1*2+e2*2+n*4,-n,t,uu,vv));
  ++hits;
 }
 float t=7,u=8,v=9;
 assert(!rayTriangleIntersect({0,0,0},{1,0,0},{2,0,0},{0,0,1},{0,0,-1},t,u,v));
 assert(t==7 && u==8 && v==9);
 assert(rayTriangleIntersect({0,0,0},{1,0,0},{0,1,0},{0,0,1},{0,0,-1},t,u,v));
 assert(t==1 && u==0 && v==0);
 for(int w:{3,100,640})for(int h:{3,100,480})for(int x:{0,w/2,w-1})for(int y:{0,h/2,h-1}) {
  auto d=primary_ray(x,y,w,h,60);
  assert(std::abs(dotProduct(d,d)-1)<1e-5 && d.z<0);
  double scale=std::tan(3.141592653589793/6);
  assert(std::abs(d.x/-d.z-((2*(x+.5)/w-1)*w/h*scale))<1e-5);
  assert(std::abs(d.y/-d.z-((1-2*(y+.5)/h)*scale))<1e-5);
 }
 std::cout<<"PASS: "<<hits<<" constructed intersections with winding/side/parallel/miss checks; degenerate/vertex and 81 camera rays\n";
}
