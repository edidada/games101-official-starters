//
// Created by Göksu Güvendiren on 2019-05-14.
//

#include "Scene.hpp"


void Scene::buildBVH() {
    printf(" - Generating BVH...\n\n");
    this->bvh = std::make_unique<BVHAccel>(objects, 1, BVHAccel::SplitMethod::NAIVE);
}

Intersection Scene::intersect(const Ray &ray) const
{
    return this->bvh->Intersect(ray);
}

void Scene::sampleLight(Intersection &pos, float &pdf) const
{
    pos=Intersection(); pdf=0;
    double total=0;
    for(auto object:objects) if(object->hasEmit()) total+=object->getArea();
    if (!(total>0)) return;
    double choice=get_random_float()*total;
    Object* selected=nullptr;
    for(auto object:objects) if(object->hasEmit() && object->getArea()>0) {
        selected=object;
        choice-=object->getArea();
        if(choice<0) break;
    }
    if (!selected) return;
    selected->Sample(pos,pdf);
    pdf*=float(selected->getArea()/total);

}

bool Scene::trace(
        const Ray &ray,
        const std::vector<Object*> &objects,
        float &tNear, uint32_t &index, Object **hitObject)
{
    *hitObject = nullptr;
    for (uint32_t k = 0; k < objects.size(); ++k) {
        float tNearK = kInfinity;
        uint32_t indexK;
        Vector2f uvK;
        if (objects[k]->intersect(ray, tNearK, indexK) && tNearK < tNear) {
            *hitObject = objects[k];
            tNear = tNearK;
            index = indexK;
        }
    }


    return (*hitObject != nullptr);
}

// Implementation of Path Tracing
Vector3f Scene::castRay(const Ray &ray, int depth) const
{
    Vector3f radiance(0), throughput(1);
    Ray current=ray;
    for(int bounce=depth;;++bounce) {
        Intersection hit=intersect(current);
        if (!hit.happened || !hit.m) break;
        if (hit.m->hasEmission()) {
            // NEE accounts for emitter hits after a diffuse/glossy bounce.
            // Camera-visible emitters must still return their emission.
            if (bounce==0 && dotProduct(hit.normal,-current.direction)>0)
                radiance+=throughput*hit.m->getEmission();
            break;
        }
        Vector3f N=hit.normal;
        Intersection light; float lightPdf=0;
        sampleLight(light,lightPdf);
        if (lightPdf>0) {
            Vector3f delta=light.coords-hit.coords;
            float distance2=dotProduct(delta,delta);
            if (distance2>0) {
                Vector3f direction=delta/std::sqrt(distance2);
                float cosSurface=std::max(0.f,dotProduct(N,direction));
                float cosLight=std::max(0.f,dotProduct(light.normal,-direction));
                if (cosSurface>0 && cosLight>0) {
                    // End the visibility segment before the sampled emitter.
                    Vector3f start=hit.coords+N*0.001f;
                    Vector3f segment=light.coords-start;
                    float length=std::sqrt(dotProduct(segment,segment));
                    Ray shadow(start,segment/length);
                    shadow.t_max=std::max(0.f,length-0.002f);
                    if (!intersect(shadow).happened)
                        radiance+=throughput*light.emit*hit.m->eval(current.direction,direction,N)*
                            (cosSurface*cosLight/(distance2*lightPdf));
                }
            }
        }
        if (!(RussianRoulette>0) || get_random_float()>=RussianRoulette) break;
        Vector3f next=normalize(hit.m->sample(current.direction,N));
        float pdf=hit.m->pdf(current.direction,next,N);
        float cosine=std::max(0.f,dotProduct(N,next));
        if (!(pdf>0 && cosine>0)) break;
        throughput=throughput*hit.m->eval(current.direction,next,N)*
            (cosine/(pdf*RussianRoulette));
        current=Ray(hit.coords+N*0.001f,next);
    }
    return radiance;
}
