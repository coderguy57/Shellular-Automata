cmake_minimum_required(VERSION 3.5.1)

project(ShellularAutomata)
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build)
set(PROJECT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src)

set(CMAKE_CXX_STANDARD 17)

link_directories(/usr/local/lib)

file(GLOB IMGUI_SRC "${CMAKE_SOURCE_DIR}/lib/*.cpp")
include_directories(${CMAKE_SOURCE_DIR}/lib)

include_directories("${PROJECT_SOURCE_DIR}")

file(GLOB_RECURSE MAIN_SOURCES "${PROJECT_SOURCE_DIR}/*.cpp")
add_executable(ShellularAutomata ${MAIN_SOURCES} ${IMGUI_SRC})
target_link_libraries(ShellularAutomata PUBLIC GL GLEW dl glfw)