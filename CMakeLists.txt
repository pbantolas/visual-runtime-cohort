cmake_minimum_required(VERSION 3.25)
project(visual_runtime LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(core)
add_subdirectory(host/cli)
add_subdirectory(host/glfw-minimal)
