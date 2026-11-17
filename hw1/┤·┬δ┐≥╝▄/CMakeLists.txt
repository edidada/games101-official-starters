cmake_minimum_required(VERSION 3.10)
project(Rasterizer)

find_package(OpenCV REQUIRED)

set(CMAKE_CXX_STANDARD 17)

include_directories(/usr/local/include)

add_executable(Rasterizer main.cpp rasterizer.hpp rasterizer.cpp Triangle.hpp Triangle.cpp)
target_link_libraries(Rasterizer ${OpenCV_LIBRARIES})

add_library(hw1-functions OBJECT main.cpp)
target_include_directories(hw1-functions PRIVATE ${OpenCV_INCLUDE_DIRS})
target_compile_definitions(hw1-functions PRIVATE main=hw1_application_main)
add_executable(hw1-checks ../../ci/hw1-checks.cpp rasterizer.cpp Triangle.cpp $<TARGET_OBJECTS:hw1-functions>)
target_include_directories(hw1-checks PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(hw1-checks PRIVATE -UNDEBUG)
target_link_libraries(hw1-checks ${OpenCV_LIBRARIES})
enable_testing()
add_test(NAME hw1-invariants COMMAND hw1-checks)
