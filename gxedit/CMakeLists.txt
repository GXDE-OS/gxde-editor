cmake_minimum_required(VERSION 3.7)

project(gxedit)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

add_executable(gxedit "main.cpp")
install(TARGETS gxedit DESTINATION bin)
