# Root CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(EnjinMigration CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# External libraries (shared, but linked independently)
set(EXTERNAL_LIBRARIES
    # Common external dependencies that both enjin and enjin2 might need
)

# Enjin1 library (original engine)
# Note: enjin directory exists but may not have CMakeLists.txt yet
# This subdirectory is commented out until enjin is configured with CMake
# add_subdirectory(enjin)

# Enjin2 library (new engine - strictly isolated)
add_subdirectory(enjin2)

# For projects that need to use both engines:
# - enjin1 will be linked with PUBLIC includes when it has CMake support
# - enjin2 uses PRIVATE includes for strict isolation
# - Both can be built independently: cmake --build build --target enjin1 / enjin2
