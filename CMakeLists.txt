cmake_minimum_required(VERSION 3.10)
project(SimpleChatProgram)

set(CMAKE_CXX_STANDARD 11)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)


add_executable(Server Server.cpp Message.h User.h User.cpp Message.cpp)
add_executable(Client Client.cpp Message.h User.h User.cpp Message.cpp)

target_link_libraries(Server Threads::Threads)
target_link_libraries(Client Threads::Threads)
