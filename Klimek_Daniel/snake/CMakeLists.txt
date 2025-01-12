cmake_minimum_required(VERSION 3.10)


project(SnakeGame C)


set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)


add_executable(server server.c)
add_executable(client client.c)


add_library(shared SHARED shared.h server.c)


find_package(Threads REQUIRED)
find_package(Curses REQUIRED)


target_link_libraries(server Threads::Threads)
target_link_libraries(client Threads::Threads ${CURSES_LIBRARIES} shared)
include_directories(${CURSES_INCLUDE_PATH})

