cmake_minimum_required(VERSION 3.10)
project(md_feeder)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_COMPILER clang++)

find_package(Boost 1.83 REQUIRED COMPONENTS system thread)
include_directories(${Boost_INCLUDE_DIRS})

add_executable(md_feeder
    md_feeder/main.cpp
)

target_link_libraries(md_feeder
    ${Boost_LIBRARIES}
    ssl 
    crypto
    pthread
)
