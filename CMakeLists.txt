cmake_minimum_required(VERSION 3.10)
project(SimpleChatProgram)

set(CMAKE_CXX_STANDARD 11)

add_executable(Server Server.cpp Message.cpp)
add_executable(Client Client.cpp Message.h User.h User.cpp Message.cpp)
