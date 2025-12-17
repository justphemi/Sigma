cmake_minimum_required(VERSION 3.10)
project(Sigma)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall")

include_directories(include)

add_executable(sig
    compiler/main.cpp
)

install(TARGETS sig DESTINATION /usr/local/bin)