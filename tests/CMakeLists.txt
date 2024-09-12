cmake_minimum_required(VERSION 3.10)
project(netkit)

add_executable(echo_server echo_server.cpp)
target_link_libraries(echo_server PRIVATE netkit_static)

add_executable(echo_client echo_client.cpp)
target_link_libraries(echo_client PRIVATE netkit_static)

add_executable(echo_server_callback echo_server_callback.cpp)
target_link_libraries(echo_server_callback PRIVATE netkit_static)

add_executable(stress_test_echo_client stress_test_echo_client.cpp)
target_link_libraries(stress_test_echo_client PRIVATE netkit_static)

add_executable(test_timer test_timer.cpp)
target_link_libraries(test_timer PRIVATE netkit_static)
