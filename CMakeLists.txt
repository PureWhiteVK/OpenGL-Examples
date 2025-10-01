cmake_minimum_required(VERSION 3.25)
project(hello-glbinding CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(glbinding)
find_package(glfw3)
find_package(glm)
find_package(fmt)
find_package(tinyobjloader)
find_package(imgui)
find_package(nfd)
find_package(libuv)

add_subdirectory(src)
