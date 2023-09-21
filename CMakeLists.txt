cmake_minimum_required(VERSION 3.14)
project(netkit)

# ----- options ----- #

option(NETKIT_BUILD_TESTS "build tests" ON)
option(NETKIT_INSTALL "install headers and libs" ON)
option(NETKIT_HOLD_DEPS "do not update existing deps" OFF)

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 14)
endif()

# ----- targets ----- #

file(GLOB_RECURSE __NETKIT_SRC__ ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_library(netkit_static STATIC ${__NETKIT_SRC__})
unset(__NETKIT_SRC__)

target_include_directories(netkit_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(MSVC)
    target_compile_options(netkit_static PRIVATE /W4)
else()
    target_compile_options(netkit_static PRIVATE -Wall -Wextra -Werror)
endif()

# ----- dependencies ----- #

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
if(NETKIT_HOLD_DEPS)
    set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
endif()

if(NOT NETKIT_DEPS_DIR)
    get_filename_component(NETKIT_DEPS_DIR ${CMAKE_CURRENT_BINARY_DIR}/deps ABSOLUTE)
    message("-- `NETKIT_DEPS_DIR` is not set. set `NETKIT_DEPS_DIR` to `${NETKIT_DEPS_DIR}`.")
endif()

macro(netkit_import_git_dep name repo commit)
    FetchContent_Declare(${name}
        GIT_REPOSITORY ${repo}
        GIT_TAG ${commit}
        SOURCE_DIR ${NETKIT_DEPS_DIR}/${name}
        BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/${name}-build
        SUBBUILD_DIR ${NETKIT_DEPS_DIR}/${name}-subbuild)
    FetchContent_MakeAvailable(${name})
endmacro()

if(NOT TARGET logger_static)
    set(LOGGER_DEPS_DIR ${NETKIT_DEPS_DIR})
    set(LOGGER_BUILD_TESTS OFF)
    set(LOGGER_INSTALL ${NETKIT_INSTALL})
    set(LOGGER_HOLD_DEPS ${NETKIT_HOLD_DEPS})
    netkit_import_git_dep(logger "https://github.com/ouonline/logger.git" master)
endif()

if(NOT TARGET liburing_static)
    netkit_import_git_dep(liburing "https://github.com/axboe/liburing.git" master)

    include(ProcessorCount)
    ProcessorCount(__NPROC__)
    execute_process(COMMAND make -j${__NPROC__} -C ${liburing_SOURCE_DIR})
    unset(__NPROC__)
    set(LIBURING_INCS ${liburing_SOURCE_DIR}/src/include)
    set(LIBURING_LIBS ${liburing_SOURCE_DIR}/src/liburing.a)
endif()

target_include_directories(netkit_static PUBLIC ${LIBURING_INCS})
target_link_libraries(netkit_static PUBLIC logger_static ${LIBURING_LIBS})

# ----- installations ----- #

if(NETKIT_INSTALL)
    install(DIRECTORY include DESTINATION .)
    install(TARGETS netkit_static DESTINATION lib)
    install(DIRECTORY ${LIBURING_INCS} DESTINATION .)
    install(FILES ${LIBURING_LIBS} DESTINATION lib)
endif()

# ----- tests ----- #

if(NETKIT_BUILD_TESTS)
    add_subdirectory(tests)
endif()
