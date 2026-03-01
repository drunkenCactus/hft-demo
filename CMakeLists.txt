cmake_minimum_required(VERSION 3.10)
project(hft)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Boost 1.83 REQUIRED COMPONENTS system thread)
find_package(GTest REQUIRED)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${Boost_INCLUDE_DIRS}
)

add_executable(md_feeder
    md_feeder/main.cpp
)

add_executable(trading_engine
    trading_engine/main.cpp
)

add_executable(run_tests
    tests/main.cpp
    tests/test_objects_handler.cpp
    tests/test_ring_buffer.cpp
    tests/test_shared_memory.cpp
)

target_link_libraries(md_feeder
    ${Boost_LIBRARIES}
    ssl 
    crypto
    pthread
    c++
    c++abi
)

target_link_libraries(trading_engine
    ${Boost_LIBRARIES}
    c++
    c++abi
)

target_link_libraries(run_tests
    GTest::GTest
    GTest::Main
)
