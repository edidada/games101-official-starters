#include "rope.h"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
using namespace CGL;
namespace {
void require(bool ok,const char* text) { if(!ok) throw std::runtime_error(text); }
void close(double a,double b,double tolerance,const char* text) { require(std::abs(a-b)<=tolerance,text); }
void topology() {
    for(int n:{2,3,16,64}) {
        Rope rope(Vector2D(2,3),Vector2D(10,7),n,2,100,{0,n-1});
        require(rope.masses.size()==size_t(n)&&rope.springs.size()==size_t(n-1),"topology sizes");
        close(rope.masses.front()->position.x,2,0,"start");close(rope.masses.back()->position.x,10,0,"end");
        for(int i=0;i<n;++i) {
            close(rope.masses[i]->position.y,3+4.*i/(n-1),1e-12,"uniform spacing");
            require(rope.masses[i]->pinned==(i==0||i==n-1),"pin flags");
            require(rope.masses[i]->velocity.norm2()==0&&rope.masses[i]->forces.norm2()==0,"initial state");
            if(i) require(rope.springs[i-1]->m1==rope.masses[i-1]&&rope.springs[i-1]->m2==rope.masses[i],"adjacent links");
        }
    }
    int rejected=0;
    for(int kind=0;kind<6;++kind) try {
        Rope bad(Vector2D(),Vector2D(1,0),kind==0?1:2,kind==1?0:1,kind==2?-1:1,
                 kind==3?std::vector<int>{-1}:(kind==4?std::vector<int>{2}:std::vector<int>{}));
        if(kind==5) bad.simulateEuler(0,Vector2D());
    } catch(const std::invalid_argument&) { ++rejected; }
    require(rejected==6,"invalid inputs accepted");
    Mass a(Vector2D(),1,false),b(Vector2D(1,0),1,false);Spring spring(&a,&b,1);
    std::vector<Mass*> points{&a,&b};std::vector<Spring*> links{&spring};
    { Rope borrowed(points,links);borrowed.simulateEuler(0.01f,Vector2D()); }
    require(a.position.x==0&&b.position.x==1,"borrowed ownership/state");
    std::cout<<"TOPOLOGY sizes=4 invalid=6 borrowed_lifetime=pass\n";
}
void oneStep() {
    float dt=0.125f;Vector2D gravity(0,-8);
    for(auto method:{Rope::EulerMethod::Explicit,Rope::EulerMethod::SemiImplicit}) {
        Rope rope(Vector2D(),Vector2D(1,0),2,2,4,{0});rope.euler_damping=0;
        rope.masses[1]->position.x=1.5;rope.masses[1]->velocity=Vector2D(2,0);
        rope.masses[0]->forces=Vector2D(9,9);
        rope.simulateEuler(dt,gravity,method);
        close(rope.masses[1]->velocity.x,2-dt,1e-12,"Hooke velocity sign/mass");
        close(rope.masses[1]->velocity.y,-8*dt,1e-12,"gravity acceleration");
        double vx=method==Rope::EulerMethod::Explicit?2:2-dt;
        close(rope.masses[1]->position.x,1.5+vx*dt,1e-12,"Euler order");
        close(rope.masses[1]->position.y,method==Rope::EulerMethod::Explicit?0:-8*dt*dt,1e-12,"Euler gravity order");
        require(rope.masses[0]->position.norm2()==0&&rope.masses[0]->forces.norm2()==0,"pinned/force reset");
        require(rope.masses[1]->forces.norm2()==0,"free force reset");
    }
    Rope free(Vector2D(),Vector2D(1,0),2,2,0,{});free.euler_damping=0.5;
    for(auto m:free.masses) m->velocity=Vector2D(2,0);
    free.simulateEuler(dt,Vector2D());
    close(free.masses[1]->velocity.x,2*(1-0.5/2*dt),1e-12,"Euler drag force");
    Rope verlet(Vector2D(),Vector2D(1,0),2,1,0,{0});
    verlet.masses[1]->last_position=Vector2D(1-dt,0);
    verlet.simulateVerlet(dt,gravity);
    close(verlet.masses[1]->position.x,1+(1-0.00005)*dt,1e-12,"Verlet default damping");
    close(verlet.masses[1]->position.y,-8*dt*dt,1e-12,"Verlet acceleration");
    close(verlet.masses[1]->last_position.x,1,0,"Verlet history");
    Rope pair(Vector2D(),Vector2D(1,0),2,1,10,{});pair.euler_damping=0;
    pair.masses[1]->position.x=2;pair.simulateEuler(dt,Vector2D());
    close(pair.masses[0]->velocity.x+pair.masses[1]->velocity.x,0,1e-12,"Newton equal and opposite");
    close(pair.masses[0]->position.x+pair.masses[1]->position.x,2,1e-12,"center of mass");
    Rope coincident(Vector2D(),Vector2D(),2,1,10,{});
    coincident.simulateEuler(dt,Vector2D());coincident.simulateVerlet(dt,Vector2D());
    require(std::isfinite(coincident.masses[0]->position.x),"coincident nonfinite");
    std::cout<<"STEP explicit=pass semi=pass verlet=pass drag=pass momentum=pass pins=pass\n";
}
double oscillator(int method,int steps,double duration,bool damping=false) {
    Rope rope(Vector2D(),Vector2D(1,0),2,1,4,{0});
    rope.euler_damping=damping?0.5:0;rope.verlet_damping=damping?0.00005:0;
    float dt=float(duration/steps);
    rope.masses[1]->position.x=1.2;
    rope.masses[1]->last_position.x=1+0.2*std::cos(2*double(dt));
    for(int i=0;i<steps;++i) {
        if(method==2) rope.simulateVerlet(dt,Vector2D());
        else rope.simulateEuler(dt,Vector2D(),method==0?Rope::EulerMethod::Explicit:Rope::EulerMethod::SemiImplicit);
    }
    return rope.masses[1]->position.x;
}
void convergence() {
    double exact=1+0.2*std::cos(2.);
    for(int method=0;method<3;++method) {
        double previous=0;
        for(int steps:{64,128,256,512}) {
            double error=std::abs(oscillator(method,steps,1)-exact);
            if(previous) {
                double ratio=previous/error;
                require(method==2?(ratio>3.6&&ratio<4.4):(ratio>1.7&&ratio<2.3),"unexpected convergence order");
            }
            std::cout<<"CONVERGENCE method="<<method<<" steps="<<steps<<" position_error="<<error<<'\n';previous=error;
        }
    }
    for(int method=0;method<3;++method) {
        Rope rope(Vector2D(),Vector2D(1,0),2,1,4,{0});rope.euler_damping=0;rope.verlet_damping=0;
        rope.masses[1]->position.x=1.2;float dt=0.05f;
        rope.masses[1]->last_position.x=1+0.2*std::cos(2*double(dt));
        for(int i=0;i<100;++i) {
            if(method==2) rope.simulateVerlet(dt,Vector2D());
            else rope.simulateEuler(dt,Vector2D(),method==0?Rope::EulerMethod::Explicit:Rope::EulerMethod::SemiImplicit);
        }
        auto m=rope.masses[1];
        // Verlet stores a half-step velocity; use energy bounds accommodating it.
        double energy=0.5*m->velocity.norm2()+2*std::pow(m->position.x-1,2);
        require(method==0?energy>0.08*2:(energy>0.08*0.85&&energy<0.08*1.15),"undamped energy behavior");
        std::cout<<"ENERGY method="<<method<<" final="<<energy<<" initial=0.08\n";
    }
    Rope damped(Vector2D(),Vector2D(1,0),2,1,4,{0});damped.masses[1]->position.x=1.2;
    float dt=0.01f;damped.masses[1]->last_position.x=1+0.2*std::cos(2*double(dt));
    for(int i=0;i<50000;++i) damped.simulateVerlet(dt,Vector2D());
    auto m=damped.masses[1];double energy=0.5*m->velocity.norm2()+2*std::pow(m->position.x-1,2);
    require(energy<0.08*0.15,"default Verlet damping did not dissipate energy");
    std::cout<<"DAMPING default_verlet_50000_steps energy="<<energy<<'\n';
}
void constraints() {
    Rope pair(Vector2D(),Vector2D(1,0),2,1,0,{});pair.verlet_damping=0;
    pair.masses[1]->position.x=2;pair.masses[1]->last_position.x=2;
    pair.simulateConstraints(0.01f,Vector2D(),1);
    close(pair.masses[0]->position.x,0.5,1e-12,"constraint equal split left");
    close(pair.masses[1]->position.x,1.5,1e-12,"constraint equal split right");
    Rope pinned(Vector2D(),Vector2D(1,0),2,1,0,{0});
    pinned.masses[1]->position.x=2;pinned.masses[1]->last_position.x=2;
    pinned.simulateConstraints(0.01f,Vector2D(),1);
    close(pinned.masses[1]->position.x,1,1e-12,"constraint pinned full correction");
    pinned.masses[1]->position=Vector2D();pinned.masses[1]->last_position=Vector2D();
    pinned.simulateConstraints(0.01f,Vector2D(),1);
    close(pinned.masses[1]->position.x,1,1e-12,"coincident constraint recovery");
    Rope chain(Vector2D(),Vector2D(7,0),8,1,0,{0});
    for(int i=0;i<10000;++i) chain.simulateConstraints(1.f/128,Vector2D(0,-9.8),64);
    double maxError=0;
    for(auto s:chain.springs) maxError=std::max(maxError,std::abs((s->m2->position-s->m1->position).norm()-s->rest_length));
    require(maxError<0.002&&chain.masses[0]->position.norm2()==0,"constraint chain drift");
    std::cout<<"CONSTRAINT chain_steps=10000 max_length_error="<<maxError<<" pins=pass\n";
}
}
int main() {
    try { std::cout<<std::setprecision(15);topology();oneStep();convergence();constraints(); }
    catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
