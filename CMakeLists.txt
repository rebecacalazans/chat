cmake_minimum_required(VERSION 2.8)
project("chat")

add_definitions(-std=c++11 )

add_executable(server.out server.cpp)
add_executable(client.out client.cpp)
target_link_libraries(server.out -pthread)
target_link_libraries(client.out -pthread)
