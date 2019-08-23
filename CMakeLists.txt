cmake_minimum_required(VERSION 3.14)
project(nettest_client)

set(CMAKE_CXX_STANDARD 11)

include_directories(.)

add_executable(nettest_client
        main.cpp
        nettest.h
        ping.cpp
        ping.h)
