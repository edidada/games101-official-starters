cmake_minimum_required(VERSION 3.10)
project(BezierCurve)

find_package(OpenCV REQUIRED)

set(CMAKE_CXX_STANDARD 14)

add_executable(BezierCurve main.cpp)

target_link_libraries(BezierCurve ${OpenCV_LIBRARIES})

option(HW4_BUILD_CHECKS "Build repository verification" OFF)
if(HW4_BUILD_CHECKS)
add_library(hw4-functions OBJECT main.cpp)
target_compile_definitions(hw4-functions PRIVATE main=hw4_application_main)
target_include_directories(hw4-functions PRIVATE ${OpenCV_INCLUDE_DIRS})
add_executable(hw4-checks ../../ci/hw4-checks.cpp $<TARGET_OBJECTS:hw4-functions>)
target_compile_options(hw4-checks PRIVATE -UNDEBUG)
target_link_libraries(hw4-checks ${OpenCV_LIBRARIES})
endif()
