cmake_minimum_required(VERSION 3.27)
project(NetworksCommunicationAssignment3 C)

set(CMAKE_C_STANDARD 11)

add_executable(NetworksCommunicationAssignment3
        RUDP.c
        RUDP_API.h
        RUDP_Receiver.c)
