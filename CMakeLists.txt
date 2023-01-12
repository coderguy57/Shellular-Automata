cmake_minimum_required(VERSION 3.15)

project(ShellularAutomata)
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build)
set(PROJECT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src)
option(BUILD_SHARED_LIBS "Build dependencies as shared libraries" OFF)
set(CMAKE_CXX_STANDARD 17)
# Set MSVC runtime to MultiThreaded (/MT)
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

link_directories(/usr/local/lib)

#file(GLOB IMGUI_SRC "${CMAKE_SOURCE_DIR}/lib/*.cpp")
#include_directories(${CMAKE_SOURCE_DIR}/lib)


include_directories("${PROJECT_SOURCE_DIR}")

file(GLOB_RECURSE MAIN_SOURCES "${PROJECT_SOURCE_DIR}/*.cpp")
add_executable(ShellularAutomata ${MAIN_SOURCES})
target_compile_options(ShellularAutomata PRIVATE
	$<$<CXX_COMPILER_ID:GNU>:     $<$<NOT:$<CONFIG:Debug>>:-O3>>
	$<$<CXX_COMPILER_ID:Clang>:   $<$<NOT:$<CONFIG:Debug>>:-O3>>
	$<$<CXX_COMPILER_ID:MSVC>:    $<$<NOT:$<CONFIG:Debug>>:/Ot>>)

#target_link_libraries(ShellularAutomata PUBLIC GL GLEW dl glfw)

add_subdirectory(dependencies)

target_link_libraries(ShellularAutomata PRIVATE
	dependency_OpenGL
	dependency_GLEW
	dependency_GLFW
	dependency_glm
	dependency_imgui)