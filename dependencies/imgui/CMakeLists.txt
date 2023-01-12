message(STATUS "Fetching imgui...")

FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui
        GIT_TAG 55d35d8387c15bf0cfd71861df67af8cfbda7456 # 1.85
)

FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
        "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
        "${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp"
        "${imgui_SOURCE_DIR}/imgui_demo.cpp"
        "${imgui_SOURCE_DIR}/imgui_draw.cpp"
        "${imgui_SOURCE_DIR}/imgui_tables.cpp"
        "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
        "${imgui_SOURCE_DIR}/imgui.cpp")
target_include_directories(imgui PUBLIC "${imgui_SOURCE_DIR}" "${imgui_SOURCE_DIR}/misc/cpp/" INTERFACE "${imgui_SOURCE_DIR}/backends")
target_compile_definitions(imgui PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLEW GL_GLEXT_PROTOTYPES=1)
target_link_libraries(imgui PRIVATE
        dependency_GLFW
        dependency_GLEW
        dependency_OpenGL)

add_library(dependency_imgui INTERFACE)
target_include_directories(dependency_imgui SYSTEM INTERFACE $<TARGET_PROPERTY:imgui,INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(dependency_imgui INTERFACE imgui)