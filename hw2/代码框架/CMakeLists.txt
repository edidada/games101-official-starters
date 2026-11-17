cmake_minimum_required(VERSION 3.10)
project(Rasterizer)

find_package(OpenCV REQUIRED)

set(CMAKE_CXX_STANDARD 17)

include_directories(/usr/local/include)

add_executable(Rasterizer main.cpp rasterizer.hpp rasterizer.cpp global.hpp Triangle.hpp Triangle.cpp)
target_link_libraries(Rasterizer ${OpenCV_LIBRARIES})

option(HW2_BUILD_CHECKS "Build independent scene driver" OFF)
if(HW2_BUILD_CHECKS)
add_executable(hw2-scenes ../../ci/hw2-scenes.cpp rasterizer.cpp Triangle.cpp)
target_include_directories(hw2-scenes PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(hw2-scenes ${OpenCV_LIBRARIES})
endif()
