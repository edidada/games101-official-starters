#pragma once
#include "Scene.hpp"
#include <cstdint>
#include <string>
class Renderer {
public:
    void Render(const Scene& scene, int spp=16, int threads=1, uint64_t seed=7,
                const std::string& output="binary.ppm");
};
