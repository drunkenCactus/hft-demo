cmake_minimum_required(VERSION 3.14)

set(CMAKE_CXX_COMPILER clang++-22)

project(hft)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -stdlib=libc++")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Boost 1.90 REQUIRED COMPONENTS thread)

include(FetchContent)
FetchContent_Declare(
    rapidjson
    GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
    GIT_TAG        24b5e7a8b27f42fa16b96fc70aade9106cf7102f
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(rapidjson)
if(NOT rapidjson_POPULATED)
    FetchContent_Populate(rapidjson)
endif()

set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.17.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(googletest)

include_directories(
    ${rapidjson_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${Boost_INCLUDE_DIRS}
)

add_library(hot_path_logger
    STATIC
    lib/log_common.cpp
    lib/interprocess/hot_path_logger.cpp
    lib/log_common.hpp
    lib/interprocess/hot_path_logger.hpp
)

add_library(logger
    STATIC
    lib/log_common.cpp
    lib/logger.cpp
    lib/log_common.hpp
    lib/logger.hpp
)

add_executable(feeder
    lib/binance/binance_ws_client.cpp
    lib/binance/parser.cpp
    feeder/main.cpp
    feeder/feeder.cpp
)

add_executable(trader
    lib/binance/binance_api_client.cpp
    lib/binance/parser.cpp
    trader/main.cpp
    trader/trader.cpp
)

add_executable(observer
    observer/main.cpp
    observer/observer.cpp
)

add_executable(run_tests
    lib/binance/parser.cpp
    tests/main.cpp
    tests/test_hot_path_logger.cpp
    tests/test_parser.cpp
    tests/test_shared_memory.cpp
    tests/test_spmc_ring_buffer.cpp
    tests/test_spsc_ring_buffer.cpp
    tests/utils.cpp
)

target_link_libraries(feeder
    ${Boost_LIBRARIES}
    ssl 
    crypto
    pthread
    c++
    c++abi
    hot_path_logger
)

target_link_libraries(trader
    ${Boost_LIBRARIES}
    ssl
    crypto
    pthread
    c++
    c++abi
    hot_path_logger
)

target_link_libraries(observer
    c++
    logger
)

target_link_libraries(run_tests
    GTest::gtest
    pthread
    c++
    c++abi
    hot_path_logger
)
