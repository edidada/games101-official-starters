#include "rope.h"
#include <cmath>
#include <memory>
#include <stdexcept>

namespace CGL {
Rope::Rope(Vector2D start,Vector2D end,int count,float mass,float k,
           std::vector<int> pins) {
    if(count<2||!std::isfinite(mass)||mass<=0||!std::isfinite(k)||k<0||
       !std::isfinite(start.x)||!std::isfinite(start.y)||
       !std::isfinite(end.x)||!std::isfinite(end.y))
        throw std::invalid_argument("Invalid rope geometry/mass/stiffness");
    std::vector<bool> pinned(count,false);
    for(int index:pins) {
        if(index<0||index>=count) throw std::invalid_argument("Pinned index out of range");
        pinned[index]=true;
    }
    std::vector<std::unique_ptr<Mass>> points;
    std::vector<std::unique_ptr<Spring>> links;
    for(int i=0;i<count;++i) {
        Vector2D position=i==count-1?end:start+(end-start)*(double(i)/(count-1));
        points.emplace_back(new Mass(position,mass,pinned[i]));
        if(i) links.emplace_back(new Spring(points[i-1].get(),points[i].get(),k));
    }
    masses.reserve(points.size());springs.reserve(links.size());
    for(auto& point:points) masses.push_back(point.release());
    for(auto& link:links) springs.push_back(link.release());
}
Rope::~Rope() {
    if(owns) {
        for(auto spring:springs) delete spring;
        for(auto mass:masses) delete mass;
    }
}
void Rope::validate(float dt,Vector2D gravity) const {
    if(!std::isfinite(dt)||dt<=0||!std::isfinite(gravity.x)||!std::isfinite(gravity.y)||
       !std::isfinite(euler_damping)||euler_damping<0||
       !std::isfinite(verlet_damping)||verlet_damping<0||verlet_damping>1)
        throw std::invalid_argument("Invalid simulation timestep/gravity/damping");
}
void Rope::springForces() {
    for(auto spring:springs) {
        Vector2D delta=spring->m2->position-spring->m1->position;
        double length=delta.norm();
        if(length==0) continue; // Coincident endpoints have no defined direction.
        Vector2D force=delta*(spring->k*(length-spring->rest_length)/length);
        spring->m1->forces+=force;
        spring->m2->forces-=force;
    }
}
void Rope::simulateEuler(float dt,Vector2D gravity,EulerMethod method) {
    validate(dt,gravity);springForces();
    for(auto point:masses) {
        if(!point->pinned) {
            Vector2D old=point->position;
            Vector2D acceleration=gravity+(point->forces-point->velocity*euler_damping)/point->mass;
            Vector2D nextVelocity=point->velocity+acceleration*dt;
            point->position+=(method==EulerMethod::Explicit?point->velocity:nextVelocity)*dt;
            point->last_position=old;
            point->velocity=nextVelocity;
        } else { point->velocity=Vector2D();point->last_position=point->position; }
        point->forces=Vector2D();
    }
}
void Rope::simulateVerlet(float dt,Vector2D gravity) {
    validate(dt,gravity);springForces();
    for(auto point:masses) {
        if(!point->pinned) {
            Vector2D old=point->position;
            Vector2D acceleration=gravity+point->forces/point->mass;
            point->position+=(point->position-point->last_position)*(1-verlet_damping)+acceleration*(double(dt)*dt);
            point->last_position=old;
            point->velocity=(point->position-old)/dt;
        } else { point->velocity=Vector2D();point->last_position=point->position; }
        point->forces=Vector2D();
    }
}
void Rope::simulateConstraints(float dt,Vector2D gravity,int iterations) {
    validate(dt,gravity);
    if(iterations<1) throw std::invalid_argument("Positive constraint iterations required");
    // Predict without Hooke forces: these links represent infinite stiffness.
    for(auto point:masses) {
        Vector2D old=point->position;
        if(!point->pinned)
            point->position+=(point->position-point->last_position)*(1-verlet_damping)+
                (gravity+point->forces/point->mass)*(double(dt)*dt);
        point->last_position=old;
        point->forces=Vector2D();
    }
    for(int pass=0;pass<iterations;++pass) for(auto spring:springs) {
        double w1=spring->m1->pinned?0:1.0/spring->m1->mass;
        double w2=spring->m2->pinned?0:1.0/spring->m2->mass;
        if(w1+w2==0) continue;
        Vector2D delta=spring->m2->position-spring->m1->position;
        double length=delta.norm();
        Vector2D direction;
        if(length>0) direction=delta/length;
        else {
            Vector2D initial=spring->m2->start_position-spring->m1->start_position;
            direction=initial.norm()>0?initial/initial.norm():Vector2D(1,0);
        }
        Vector2D correction=direction*((length-spring->rest_length)/(w1+w2));
        spring->m1->position+=correction*w1;
        spring->m2->position-=correction*w2;
    }
    for(auto point:masses)
        point->velocity=point->pinned?Vector2D():(point->position-point->last_position)/dt;
}
}
