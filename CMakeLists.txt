cmake_minimum_required(VERSION 3.29)
project(Progetto_API C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -std=gnu11 -lm -Wall")

add_executable(Progetto_API main.c)
