cmake_minimum_required(VERSION 3.10)
project(netkit)

option(NETKIT_BUILD_TESTS "build tests" ON)

file(GLOB NETKIT_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_library(netkit_static STATIC ${NETKIT_SRC})
target_compile_features(netkit_static PRIVATE cxx_std_11)
target_include_directories(netkit_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(LOGGER_BUILD_TESTS OFF CACHE BOOL "")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../logger ${CMAKE_CURRENT_BINARY_DIR}/logger)
target_link_libraries(netkit_static PUBLIC logger_static)

set(THREADKIT_BUILD_TESTS OFF CACHE BOOL "")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../threadkit ${CMAKE_CURRENT_BINARY_DIR}/threadkit)
target_link_libraries(netkit_static PUBLIC threadkit_static)

if(NETKIT_BUILD_TESTS)
    add_subdirectory(tests)
endif()
