#include "Renderer.hpp"
#include "Scene.hpp"
#include "Triangle.hpp"
#include <chrono>
#include <iomanip>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
    try {
        BVHAccel::SplitMethod method = BVHAccel::SplitMethod::NAIVE;
        std::string methodName = "naive";
        if (argc == 2 && std::string(argv[1]) == "sah") {
            method = BVHAccel::SplitMethod::SAH;
            methodName = "sah";
        } else if (argc > 2 || (argc == 2 && std::string(argv[1]) != "naive")) {
            throw std::invalid_argument("usage: RayTracing [naive|sah]");
        }
        using Clock = std::chrono::steady_clock;
        auto start = Clock::now();
        Scene scene(1280, 960);
        MeshTriangle bunny("../models/bunny/bunny.obj", method);
        scene.Add(&bunny);
        scene.Add(std::make_unique<Light>(Vector3f(-20, 70, 20), 1));
        scene.Add(std::make_unique<Light>(Vector3f(20, 70, 20), 1));
        scene.buildBVH(method);
        auto built = Clock::now();
        Renderer renderer;
        renderer.Render(scene);
        auto rendered = Clock::now();
        std::cout << "\nTIMING " << methodName << std::setprecision(12)
                  << " load_build_ms=" << std::chrono::duration<double, std::milli>(built-start).count()
                  << " render_ms=" << std::chrono::duration<double, std::milli>(rendered-built).count()
                  << "\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
