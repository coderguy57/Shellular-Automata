message(STATUS "Fetching GLFW...")

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw
    GIT_TAG 3.3.8
)

FetchContent_MakeAvailable(glfw)

add_library(dependency_GLFW INTERFACE)
target_include_directories(dependency_GLFW SYSTEM INTERFACE "${glfw_SOURCE_DIR}/include")
target_link_libraries(dependency_GLFW INTERFACE glfw)