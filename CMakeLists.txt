cmake_minimum_required(VERSION 3.14)
project(netkit)

# ----- options ----- #

option(NETKIT_BUILD_TESTS "build tests" ON)
option(NETKIT_INSTALL "install headers and libs" ON)

# ----- targets ----- #

file(GLOB_RECURSE __NETKIT_SRC__ ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_library(netkit_static STATIC ${__NETKIT_SRC__})
unset(__NETKIT_SRC__)

target_include_directories(netkit_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(netkit_static PUBLIC cxx_std_11)

if(MSVC)
    target_compile_options(netkit_static PRIVATE /W4)
else()
    target_compile_options(netkit_static PRIVATE -Wall -Wextra -Werror)
endif()

# ----- dependencies ----- #

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

if(NOT NETKIT_DEPS_DIR)
    get_filename_component(NETKIT_DEPS_DIR ${CMAKE_CURRENT_BINARY_DIR} ABSOLUTE)
    message("-- `NETKIT_DEPS_DIR` is not set. set `NETKIT_DEPS_DIR` to `${NETKIT_DEPS_DIR}`.")
endif()

if(NOT TARGET logger_static)
    set(LOGGER_BUILD_TESTS OFF CACHE BOOL "")
    set(LOGGER_DEPS_DIR ${NETKIT_DEPS_DIR})
    FetchContent_Declare(logger
        GIT_REPOSITORY "https://github.com/ouonline/logger.git"
        GIT_TAG master
        SOURCE_DIR ${NETKIT_DEPS_DIR}/logger
        BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/logger-build
        SUBBUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/logger-subbuild)
    FetchContent_MakeAvailable(logger)
endif()

if(NOT TARGET threadkit_static)
    set(THREADKIT_BUILD_TESTS OFF CACHE BOOL "")
    set(THREADKIT_DEPS_DIR ${NETKIT_DEPS_DIR})
    FetchContent_Declare(threadkit
        GIT_REPOSITORY "https://github.com/ouonline/threadkit.git"
        GIT_TAG master
        SOURCE_DIR ${NETKIT_DEPS_DIR}/threadkit
        BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/threadkit-build
        SUBBUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/threadkit-subbuild)
    FetchContent_MakeAvailable(threadkit)
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
