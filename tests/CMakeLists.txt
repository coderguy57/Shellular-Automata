cmake_minimum_required(VERSION 3.15)

file(GLOB_RECURSE MAIN_SOURCES
	${PROJECT_SOURCE_DIR}/render/*.cpp
	${PROJECT_SOURCE_DIR}/utils/*.cpp
	${PROJECT_SOURCE_DIR}/engines/*.cpp
	${PROJECT_SOURCE_DIR}/data.cpp
)
add_executable(tests_project ${MAIN_SOURCES} "tests.cpp" "dynamic_control_tests.cpp" "recording_tests.cpp" "shader_control_tests.cpp" "fullscreen_pass_tests.cpp" "slack_automata_tests.cpp" "neural_mnca_tests.cpp" "window_loop_tests.cpp"
    ${PROJECT_SOURCE_DIR}/controllers/dynamic_control.cpp
    ${PROJECT_SOURCE_DIR}/controllers/engine_control.cpp
    ${PROJECT_SOURCE_DIR}/controllers/init_texture_control.cpp
    ${PROJECT_SOURCE_DIR}/controllers/mutation_control.cpp
    ${PROJECT_SOURCE_DIR}/controllers/mutation_control_NN.cpp
    ${PROJECT_SOURCE_DIR}/controllers/neural_mutation_control.cpp
    ${PROJECT_SOURCE_DIR}/controllers/paint_control.cpp
    ${PROJECT_SOURCE_DIR}/controllers/recorder_control.cpp
    ${PROJECT_SOURCE_DIR}/controllers/shader_control.cpp
    ${PROJECT_SOURCE_DIR}/controllers/statistics_control.cpp
    ${PROJECT_SOURCE_DIR}/controllers/viewer_control.cpp
    ${PROJECT_SOURCE_DIR}/controllers/window_control.cpp
    ${PROJECT_SOURCE_DIR}/setup/create_setup.cpp
    ${PROJECT_SOURCE_DIR}/setup/mnca_nn_setup.cpp
    ${PROJECT_SOURCE_DIR}/setup/neural_mnca_setup.cpp
    ${PROJECT_SOURCE_DIR}/simulation_setup.cpp
    ${PROJECT_SOURCE_DIR}/simulation_setup_loader.cpp
    ${PROJECT_SOURCE_DIR}/viewer.cpp
    ${PROJECT_SOURCE_DIR}/window.cpp
)

target_link_libraries(tests_project
    PRIVATE
    Catch2::Catch2
)

# Add memory leak detection to all tests
# add_test(NAME MemoryLeaks COMMAND valgrind --leak-check=full --error-exitcode=1 $<TARGET_FILE:tests_project>)

target_link_libraries(tests_project PRIVATE
	dependency_OpenGL
	dependency_GLEW
	dependency_GLFW
	dependency_glm
	dependency_imgui
	dependency_yaml
	Catch2::Catch2WithMain
    dependency_FFTW
)

# list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/contrib)

target_include_directories(tests_project PRIVATE ${PROJECT_SOURCE_DIR})

# include(CTest)
include(${CMAKE_SOURCE_DIR}/dependencies/Catch2/extras/Catch.cmake)
catch_discover_tests(tests_project WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_test(NAME slack_shaders_build_directory
    COMMAND tests_project "[slack]")
set_tests_properties(slack_shaders_build_directory PROPERTIES
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    SKIP_RETURN_CODE 4)

add_test(NAME neural_shaders_build_directory
    COMMAND tests_project "[neural]")
set_tests_properties(neural_shaders_build_directory PROPERTIES
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    SKIP_RETURN_CODE 4)
