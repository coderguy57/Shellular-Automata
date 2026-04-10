# message(STATUS "Fetching FFTW...")

# # Fetch FFTW tarball
# FetchContent_Declare(
#     FFTW
#     URL http://fftw.org/fftw-3.3.10.tar.gz
#     URL_HASH MD5=8ccbf6a5ea78a16dbc3e1306e234cc5c
#     DOWNLOAD_EXTRACT_TIMESTAMP TRUE
# )

# # Build FFTW with float support
# set(ENABLE_FLOAT ON CACHE BOOL "Build static library" FORCE)
# set(ENABLE_STATIC ON CACHE BOOL "Build static library")

# add_library(dependency_FFTW INTERFACE)
# target_include_directories(dependency_FFTW SYSTEM INTERFACE "${fftw_SOURCE_DIR}/api")
# target_link_libraries(dependency_FFTW INTERFACE fftw3)
message(STATUS "Fetching FFTW...")

# Fetch FFTW tarball
FetchContent_Declare(
    FFTW
    URL http://fftw.org/fftw-3.3.10.tar.gz
    URL_HASH MD5=8ccbf6a5ea78a16dbc3e1306e234cc5c
)

set(ENABLE_FLOAT ON)

FetchContent_MakeAvailable(FFTW)

add_library(dependency_FFTW INTERFACE)
target_include_directories(dependency_FFTW SYSTEM INTERFACE "${fftw_SOURCE_DIR}/api")
target_link_libraries(dependency_FFTW INTERFACE fftw3f)