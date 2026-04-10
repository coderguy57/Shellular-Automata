cmake_minimum_required(VERSION 3.15)

file(GLOB_RECURSE MAIN_SOURCES
	${PROJECT_SOURCE_DIR}/render/*.cpp
	${PROJECT_SOURCE_DIR}/utils/*.cpp
	${PROJECT_SOURCE_DIR}/engines/*.cpp
	# ${PROJECT_SOURCE_DIR}/controllers/*.cpp
	${PROJECT_SOURCE_DIR}/data.cpp
	# ${PROJECT_SOURCE_DIR}/window.cpp
	# ${PROJECT_SOURCE_DIR}/simulation_setup.cpp
	# ${PROJECT_SOURCE_DIR}/simulation_setup_loader.cpp
	# ${PROJECT_SOURCE_DIR}/viewer.cpp
)
add_executable(tests_project ${MAIN_SOURCES} "tests.cpp")

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
catch_discover_tests(tests_project)

file(COPY ${CMAKE_SOURCE_DIR}/tests/res
     DESTINATION ${CMAKE_BINARY_DIR}/tests)
file(COPY ${CMAKE_SOURCE_DIR}/res
     DESTINATION ${CMAKE_BINARY_DIR})