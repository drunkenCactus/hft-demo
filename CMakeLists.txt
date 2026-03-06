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

set(COMMON_SOURCES
    lib/log_common.cpp
    lib/logger.cpp
    lib/interprocess/hot_path_logger.cpp
)

set(COMMON_HEADERS
    lib/log_common.hpp
    lib/logger.hpp
    lib/interprocess/hot_path_logger.hpp
)

add_library(common_lib STATIC ${COMMON_SOURCES} ${COMMON_HEADERS})

add_executable(md_feeder
    md_feeder/main.cpp
)

add_executable(trading_engine
    trading_engine/main.cpp
)

add_executable(observer
    observer/main.cpp
    observer/observer.cpp
)

add_executable(run_tests
    tests/main.cpp
    tests/test_hot_path_logger.cpp
    tests/test_shared_memory.cpp
    tests/test_spmc_ring_buffer.cpp
    tests/test_spsc_ring_buffer.cpp
    tests/utils.cpp
)

target_link_libraries(md_feeder
    ${Boost_LIBRARIES}
    ssl 
    crypto
    pthread
    c++
    c++abi
    common_lib
)

target_link_libraries(trading_engine
    ${Boost_LIBRARIES}
    c++
    c++abi
    common_lib
)

target_link_libraries(observer
    c++
    common_lib
)

target_link_libraries(run_tests
    GTest::GTest
    GTest::Main
    common_lib
)
