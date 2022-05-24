cmake_minimum_required(VERSION 3.10)
project(netkit)

# ----- options ----- #

option(NETKIT_BUILD_TESTS "build tests" ON)
option(NETKIT_INSTALL "install headers and libs" ON)

# ----- targets ----- #

file(GLOB NETKIT_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_library(netkit_static STATIC ${NETKIT_SRC})
target_include_directories(netkit_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# ----- dependencies ----- #

if(NOT NETKIT_DEPS_DIR)
    set(NETKIT_DEPS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
    message(WARNING "`NETKIT_DEPS_DIR` is not set. set `NETKIT_DEPS_DIR` to `${NETKIT_DEPS_DIR}`.")
endif()

if(NOT TARGET logger_static)
    set(LOGGER_BUILD_TESTS OFF CACHE BOOL "")
    set(LOGGER_DEPS_DIR ${NETKIT_DEPS_DIR})
    add_subdirectory(${NETKIT_DEPS_DIR}/logger ${CMAKE_CURRENT_BINARY_DIR}/logger-build)
endif()

if(NOT TARGET threadkit_static)
    set(THREADKIT_BUILD_TESTS OFF CACHE BOOL "")
    set(THREADKIT_DEPS_DIR ${NETKIT_DEPS_DIR})
    add_subdirectory(${NETKIT_DEPS_DIR}/threadkit ${CMAKE_CURRENT_BINARY_DIR}/threadkit-build)
endif()

target_link_libraries(netkit_static PUBLIC logger_static threadkit_static)

# ----- installations ----- #

if(NETKIT_INSTALL)
    install(DIRECTORY include DESTINATION .)
    install(TARGETS netkit_static DESTINATION lib)
endif()

# ----- tests ----- #

if(NETKIT_BUILD_TESTS)
    add_subdirectory(tests)
endif()
