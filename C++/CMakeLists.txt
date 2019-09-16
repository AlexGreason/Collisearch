cmake_minimum_required(VERSION 3.10)
project(GameOfLife)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-O2")

add_executable(GameOfLife main.cpp)

target_include_directories(GameOfLife PRIVATE /home/exa/.conda/envs/py3.7/lib/python3.7/site-packages/lifelib)
