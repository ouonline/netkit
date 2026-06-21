cmake_minimum_required(VERSION 3.14)
project(netkit)

# ----- requirements ----- #

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "only Linux is supported currently.")
endif()

execute_process(COMMAND uname -r OUTPUT_VARIABLE __UNAME_RESULT__ OUTPUT_STRIP_TRAILING_WHITESPACE)
message(-- " Kernel version: " ${__UNAME_RESULT__})
string(REGEX MATCH "[0-9]+.[0-9]+" LINUX_KERNEL_VERSION ${__UNAME_RESULT__})
unset(__UNAME_RESULT__)

# io_uring accept support since 5.5, and multi accept support since 5.19.
set(__IOURING_MIN_KERNEL_VERSION__ 5.19)

# ----- options ----- #

option(NETKIT_BUILD_TESTS "build tests" ON)
option(NETKIT_INSTALL "install headers and libs" ON)
option(NETKIT_HOLD_DEPS "do not update existing deps" OFF)

if(LINUX_KERNEL_VERSION VERSION_LESS ${__IOURING_MIN_KERNEL_VERSION__})
    message(FATAL_ERROR "kernel version >= [${__IOURING_MIN_KERNEL_VERSION__}] is required.")
endif()

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
elseif(CMAKE_CXX_STANDARD LESS 17)
    message(FATAL_ERROR "the `new` operator for `alignas` requires C++17 at least.")
endif()

# ----- targets ----- #

file(GLOB __NETKIT_SRC__ ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB __IOURING_SRC__ ${CMAKE_CURRENT_SOURCE_DIR}/src/iouring/*.cpp)
list(APPEND __NETKIT_SRC__ ${__IOURING_SRC__})
unset(__IOURING_SRC__)
add_library(netkit_static STATIC ${__NETKIT_SRC__})
unset(__NETKIT_SRC__)

target_include_directories(netkit_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(netkit_static PRIVATE -Wall -Wextra -Werror)

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
        GIT_SHALLOW TRUE
        SOURCE_DIR ${NETKIT_DEPS_DIR}/${name}
        BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/${name}-build
        SUBBUILD_DIR ${NETKIT_DEPS_DIR}/${name}-subbuild)
    FetchContent_MakeAvailable(${name})
endmacro()

if(NOT TARGET cutils_static)
    set(CUTILS_DEPS_DIR ${NETKIT_DEPS_DIR})
    set(CUTILS_BUILD_TESTS OFF)
    set(CUTILS_INSTALL ${NETKIT_INSTALL})
    set(CUTILS_HOLD_DEPS ${NETKIT_HOLD_DEPS})
    if(NETKIT_DEP_CUTILS_SOURCE_DIR)
        add_subdirectory(${NETKIT_DEP_CUTILS_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/cutils-build)
        set(cutils_SOURCE_DIR ${NETKIT_DEP_CUTILS_SOURCE_DIR})
    else()
        netkit_import_git_dep(cutils "https://github.com/ouonline/cutils.git" master)
    endif()
endif()

if(NOT TARGET logger_static)
    set(LOGGER_DEPS_DIR ${NETKIT_DEPS_DIR})
    set(LOGGER_BUILD_TESTS OFF)
    set(LOGGER_INSTALL ${NETKIT_INSTALL})
    set(LOGGER_HOLD_DEPS ${NETKIT_HOLD_DEPS})
    if(NETKIT_DEP_LOGGER_SOURCE_DIR)
        add_subdirectory(${NETKIT_DEP_LOGGER_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/logger-build)
        set(logger_SOURCE_DIR ${NETKIT_DEP_LOGGER_SOURCE_DIR})
    else()
        netkit_import_git_dep(logger "https://github.com/ouonline/logger.git" master)
    endif()
endif()

if(NOT TARGET threadkit_static)
    set(THREADKIT_DEPS_DIR ${NETKIT_DEPS_DIR})
    set(THREADKIT_BUILD_TESTS OFF)
    set(THREADKIT_INSTALL ${NETKIT_INSTALL})
    set(THREADKIT_HOLD_DEPS ${NETKIT_HOLD_DEPS})
    if(NETKIT_DEP_THREADKIT_SOURCE_DIR)
        add_subdirectory(${NETKIT_DEP_THREADKIT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/threadkit-build)
        set(threadkit_SOURCE_DIR ${NETKIT_DEP_THREADKIT_SOURCE_DIR})
    else()
        netkit_import_git_dep(threadkit "https://github.com/ouonline/threadkit.git" master)
    endif()
endif()

target_link_libraries(netkit_static PUBLIC threadkit_static logger_static cutils_static)

if(NOT LIBURING_LIBS)
    if(NETKIT_DEP_LIBURING_SOURCE_DIR)
        set(liburing_SOURCE_DIR ${NETKIT_DEP_LIBURING_SOURCE_DIR})
    else()
        netkit_import_git_dep(liburing "https://github.com/axboe/liburing.git" master)
    endif()

    include(ProcessorCount)
    ProcessorCount(__NPROC__)
    execute_process(COMMAND make -j${__NPROC__} -C ${liburing_SOURCE_DIR})
    unset(__NPROC__)
    set(LIBURING_INCS ${liburing_SOURCE_DIR}/src/include)
    set(LIBURING_LIBS ${liburing_SOURCE_DIR}/src/liburing.a)
endif()
target_include_directories(netkit_static PUBLIC ${LIBURING_INCS})
target_link_libraries(netkit_static PUBLIC ${LIBURING_LIBS})

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
