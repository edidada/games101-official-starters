#pragma once
#include "Scene.hpp"

struct hit_payload
{
    float tNear;
    uint32_t index;
    Vector2f uv;
    Object* hit_obj;
};

class Renderer
{
public:
    void Render(const Scene& scene);

private:
};
Vector3f primary_ray(int x,int y,int width,int height,float fov);
