include(FetchContent)

# Avoid warning about DOWNLOAD_EXTRACT_TIMESTAMP in CMake 3.24:
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
	cmake_policy(SET CMP0135 NEW) # Needed by GLEW
endif()

add_subdirectory(OpenGL)
add_subdirectory(GLEW) # Depends on OpenGL.
add_subdirectory(GLFW) # Depends on OpenGL.
add_subdirectory(glm)
add_subdirectory(imgui)
add_subdirectory(yaml)