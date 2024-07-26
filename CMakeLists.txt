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
set(__IOURING_MIN_KERNEL_VERSION__ 5.5)

# ----- options ----- #

option(NETKIT_BUILD_TESTS "build tests" ON)
option(NETKIT_INSTALL "install headers and libs" ON)
option(NETKIT_HOLD_DEPS "do not update existing deps" OFF)

if(LINUX_KERNEL_VERSION VERSION_LESS ${__IOURING_MIN_KERNEL_VERSION__})
    set(__PREFER_EPOLL__ ON)
    set(__PREFER_IOURING__ OFF)
else()
    set(__PREFER_EPOLL__ OFF)
    set(__PREFER_IOURING__ ON)
endif()

option(NETKIT_ENABLE_EPOLL "enable epoll support" ${__PREFER_EPOLL__})
option(NETKIT_ENABLE_IOURING "enable iouring support. kernel version >= ${__IOURING_MIN_KERNEL_VERSION__} required." ${__PREFER_IOURING__})

unset(__PREFER_EPOLL__)
unset(__PREFER_IOURING__)

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 14)
endif()

if(NOT NETKIT_ENABLE_EPOLL AND NOT NETKIT_ENABLE_IOURING)
    message(FATAL_ERROR "Neither `NETKIT_ENABLE_IOURING` nor `NETKIT_ENABLE_EPOLL` is enabled.")
endif()

if(NETKIT_ENABLE_IOURING)
    if(LINUX_KERNEL_VERSION VERSION_LESS ${__IOURING_MIN_KERNEL_VERSION__})
        message(FATAL_ERROR "kernel version >= ${__IOURING_MIN_KERNEL_VERSION__} is required for io_uring.")
    endif()
endif()

# ----- targets ----- #

file(GLOB __NETKIT_SRC__ ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
if(NETKIT_ENABLE_IOURING)
    file(GLOB __IOURING_SRC__ ${CMAKE_CURRENT_SOURCE_DIR}/src/iouring/*.cpp)
    list(APPEND __NETKIT_SRC__ ${__IOURING_SRC__})
    unset(__IOURING_SRC__)
endif()
if(NETKIT_ENABLE_EPOLL)
    file(GLOB __EPOLL_SRC__ ${CMAKE_CURRENT_SOURCE_DIR}/src/epoll/*.cpp)
    list(APPEND __NETKIT_SRC__ ${__EPOLL_SRC__})
    unset(__EPOLL_SRC__)
endif()
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

target_link_libraries(netkit_static PUBLIC logger_static)

if(NETKIT_ENABLE_IOURING)
    if(NOT LIBURING_LIBS)
        netkit_import_git_dep(liburing "https://github.com/axboe/liburing.git" master)

        include(ProcessorCount)
        ProcessorCount(__NPROC__)
        execute_process(COMMAND make -j${__NPROC__} -C ${liburing_SOURCE_DIR})
        unset(__NPROC__)
        set(LIBURING_INCS ${liburing_SOURCE_DIR}/src/include)
        set(LIBURING_LIBS ${liburing_SOURCE_DIR}/src/liburing.a)
    endif()
    target_include_directories(netkit_static PUBLIC ${LIBURING_INCS})
    target_link_libraries(netkit_static PUBLIC ${LIBURING_LIBS})
    target_compile_definitions(netkit_static PUBLIC NETKIT_ENABLE_IOURING)
    if(LINUX_KERNEL_VERSION VERSION_GREATER_EQUAL 5.18)
        target_compile_definitions(netkit_static PUBLIC NETKIT_IOURING_ENABLE_RING_MSG)
    endif()
    if(LINUX_KERNEL_VERSION VERSION_GREATER_EQUAL 5.19)
        target_compile_definitions(netkit_static PUBLIC NETKIT_IOURING_ENABLE_MULTI_ACCEPT)
    endif()
endif()

if(NETKIT_ENABLE_EPOLL)
    target_compile_definitions(netkit_static PUBLIC NETKIT_ENABLE_EPOLL)
endif()

# ----- installations ----- #

if(NETKIT_INSTALL)
    install(DIRECTORY include DESTINATION .)
    install(TARGETS netkit_static DESTINATION lib)
    if(NETKIT_ENABLE_IOURING)
        install(DIRECTORY ${LIBURING_INCS} DESTINATION .)
        install(FILES ${LIBURING_LIBS} DESTINATION lib)
    endif()
endif()

# ----- tests ----- #

if(NETKIT_BUILD_TESTS)
    add_subdirectory(tests)
endif()
