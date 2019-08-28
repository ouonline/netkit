cmake_minimum_required(VERSION 3.10)
project(netkit)

file(GLOB NETKIT_TEST_SRC *.cpp)
add_executable(echo_server_and_client ${NETKIT_TEST_SRC})
target_link_libraries(echo_server_and_client PRIVATE netkit_static)
