cmake_minimum_required(VERSION 3.10)
project(netkit)

add_executable(echo_server_and_client echo_server_and_client.cpp)
target_link_libraries(echo_server_and_client PRIVATE netkit_static)
