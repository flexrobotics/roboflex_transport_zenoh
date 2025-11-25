# python/CMakeLists.txt
cmake_minimum_required(VERSION 3.18)

project(roboflex_transport_zenoh_ext)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(PYBIND11_CPP_STANDARD -std=c++20)


# --------------------
# Resolve dependencies

find_package(Python REQUIRED COMPONENTS Interpreter Development NumPy)

include(FetchContent)

FetchContent_Declare(pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG        v2.11.1
)
FetchContent_MakeAvailable(pybind11)


# --------------------
# Define the library

pybind11_add_module(roboflex_transport_zenoh_ext
    pybindings.cpp
)

target_link_libraries(roboflex_transport_zenoh_ext PRIVATE
    roboflex_transport_zenoh
)

set_target_properties(roboflex_transport_zenoh_ext PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "$ORIGIN"
)
