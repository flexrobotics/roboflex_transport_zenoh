cmake_minimum_required(VERSION 3.18)

project(roboflex_transport_zenoh)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)


# --------------------
# Resolve dependencies

include(FetchContent)

# download and build roboflex_core
FetchContent_Declare(roboflex_core
    GIT_REPOSITORY https://github.com/flexrobotics/roboflex.git
    GIT_TAG        main
)
set(BUILD_ROBOFLEX_PYTHON_EXT OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(roboflex_core)

# download zenoh-c
FetchContent_Declare(zenohc
    GIT_REPOSITORY https://github.com/eclipse-zenoh/zenoh-c.git
    GIT_TAG        main
)
set(ZENOHC_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ZENOHC_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(zenohc)

set(ZENOH_TARGET zenohc)
if(NOT TARGET ${ZENOH_TARGET} AND TARGET zenohc::zenohc)
    set(ZENOH_TARGET zenohc::zenohc)
endif()
if(NOT TARGET ${ZENOH_TARGET} AND TARGET zenohc_static)
    set(ZENOH_TARGET zenohc_static)
endif()
if(NOT TARGET ${ZENOH_TARGET} AND TARGET zenohc::zenohc_static)
    set(ZENOH_TARGET zenohc::zenohc_static)
endif()


# --------------------
# Define the library

add_library(roboflex_transport_zenoh STATIC
    src/zenoh_nodes.cpp
    include/roboflex_transport_zenoh/zenoh_nodes.h
)

set_property(TARGET roboflex_transport_zenoh PROPERTY
    POSITION_INDEPENDENT_CODE ON
)

target_include_directories(roboflex_transport_zenoh PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(TARGET ${ZENOH_TARGET})
    target_link_libraries(roboflex_transport_zenoh PUBLIC
        roboflex_core
        ${ZENOH_TARGET}
    )
else()
    message(WARNING "Zenoh target not detected; roboflex_transport_zenoh will link without explicit zenoh library target")
    target_link_libraries(roboflex_transport_zenoh PUBLIC roboflex_core)
endif()


# --------------------
# Examples

add_executable(pub_sub_0_zenoh_cpp examples/pub_sub_0_cpp.cpp)
target_link_libraries(pub_sub_0_zenoh_cpp PRIVATE
    roboflex_core
    roboflex_transport_zenoh
)

add_executable(pub_sub_config_zenoh_cpp examples/pub_sub_config_cpp.cpp)
target_link_libraries(pub_sub_config_zenoh_cpp PRIVATE
    roboflex_core
    roboflex_transport_zenoh
)


# --------------------
# install

install(TARGETS roboflex_transport_zenoh
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(DIRECTORY include/roboflex_transport_zenoh
    DESTINATION include
)

install(TARGETS roboflex_transport_zenoh
    EXPORT roboflex_transport_zenohTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(EXPORT roboflex_transport_zenohTargets
    FILE roboflex_transport_zenohTargets.cmake
    DESTINATION lib/cmake/roboflex_transport_zenoh
)

include(CMakePackageConfigHelpers)
configure_package_config_file(Config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/roboflex_transport_zenohConfig.cmake
    INSTALL_DESTINATION lib/cmake/roboflex_transport_zenoh
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/roboflex_transport_zenohConfig.cmake
    DESTINATION lib/cmake/roboflex_transport_zenoh
)


# --------------------
# build python bindings

add_subdirectory(python)
