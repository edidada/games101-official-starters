cmake_minimum_required(VERSION 3.10)
project(RayTracing)

set(CMAKE_CXX_STANDARD 17)

add_executable(RayTracing main.cpp Object.hpp Vector.hpp Sphere.hpp global.hpp Triangle.hpp Scene.cpp Scene.hpp Light.hpp Renderer.cpp)
target_compile_options(RayTracing PUBLIC -Wall -Wextra -pedantic -Wshadow -Wreturn-type -fsanitize=undefined)
target_compile_features(RayTracing PUBLIC cxx_std_17)
target_link_libraries(RayTracing PUBLIC -fsanitize=undefined)

option(HW5_BUILD_CHECKS "Build repository verification" OFF)
if(HW5_BUILD_CHECKS)
add_executable(hw5-checks ../../ci/hw5-checks.cpp Renderer.cpp Scene.cpp)
target_include_directories(hw5-checks PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(hw5-checks PRIVATE -UNDEBUG)
endif()
