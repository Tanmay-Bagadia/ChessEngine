cmake_minimum_required(VERSION 3.16)
project(ChessEngine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose build type: Debug Release" FORCE)
endif()

# Compiler flags
if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Oi /Ot /DNDEBUG")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
endif()

# On macOS with Clang, ensure libc++ headers from SDK take priority over /usr/local/include
if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    execute_process(COMMAND xcrun --show-sdk-path OUTPUT_VARIABLE MACOS_SDK_PATH OUTPUT_STRIP_TRAILING_WHITESPACE)
    include_directories(SYSTEM BEFORE "${MACOS_SDK_PATH}/usr/include/c++/v1")
endif()

set(SOURCES
    engine_src/bitboard.cpp
    engine_src/position.cpp
    engine_src/movegen.cpp
    engine_src/perft.cpp
    engine_src/evaluation.cpp
    engine_src/search.cpp
    engine_src/uci.cpp
    engine_src/main.cpp
    engine_src/zobrist.cpp
)

add_executable(engine ${SOURCES})

target_include_directories(engine PRIVATE engine_src)
