message(STATUS "Fetching yaml...")

FetchContent_Declare(
	yaml
	GIT_REPOSITORY https://github.com/jbeder/yaml-cpp
	GIT_TAG		   yaml-cpp-0.7.0
)

FetchContent_MakeAvailable(yaml)

add_library(dependency_yaml INTERFACE)
target_include_directories(dependency_yaml SYSTEM INTERFACE "${yaml_SOURCE_DIR}/include")
target_link_libraries(dependency_yaml INTERFACE yaml-cpp)
