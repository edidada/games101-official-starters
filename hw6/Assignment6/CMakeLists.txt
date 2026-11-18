cmake_minimum_required(VERSION 3.13)
project(RayTracing)

set(CMAKE_CXX_STANDARD 17)

add_executable(RayTracing main.cpp Object.hpp Vector.cpp Vector.hpp Sphere.hpp global.hpp Triangle.hpp Scene.cpp
        Scene.hpp Light.hpp AreaLight.hpp BVH.cpp BVH.hpp Bounds3.hpp Ray.hpp Material.hpp Intersection.hpp
        Renderer.cpp Renderer.hpp)

option(HW6_SANITIZE "Enable address/undefined behavior sanitizers" OFF)
add_executable(hw6_tests tests.cpp BVH.cpp Vector.cpp)
foreach(target RayTracing hw6_tests)
    target_compile_options(${target} PRIVATE -Wall -Wextra)
    if(HW6_SANITIZE)
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined -no-pie)
    endif()
endforeach()
enable_testing()
add_test(NAME hw6_correctness COMMAND hw6_tests)
