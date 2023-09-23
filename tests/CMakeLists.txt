cmake_minimum_required(VERSION 3.10)
project(netkit)

add_executable(echo_server echo_server.cpp)
target_link_libraries(echo_server PRIVATE netkit_static)

add_executable(echo_client echo_client.cpp)
target_link_libraries(echo_client PRIVATE netkit_static)
