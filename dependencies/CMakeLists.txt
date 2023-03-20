include(FetchContent)

add_subdirectory(OpenGL)
add_subdirectory(GLEW) # Depends on OpenGL.
add_subdirectory(GLFW) # Depends on OpenGL.
add_subdirectory(glm)
add_subdirectory(imgui)