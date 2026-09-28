# CMakeLists.txt
# PROVIDED — do not modify unless you add new source files of your own
# (e.g. a separate message.cpp), in which case add them to CORE_SOURCES.

cmake_minimum_required(VERSION 3.16)
project(MiniHarness VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable strict warnings and AddressSanitizer.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -fsanitize=address,undefined -g")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address,undefined")

include_directories(include)

# 1. Define the core files (everything EXCEPT main.cpp).
set(CORE_SOURCES
    src/conversation.cpp
    src/sentinel_scanner.cpp
    src/model_client.cpp
    src/scripted_client.cpp
    src/replay_client.cpp
    src/harness.cpp
)

# 2. Build the main program.
add_executable(miniharness ${CORE_SOURCES} src/main.cpp)

# 3. Build the test suite.
add_executable(test_p2 ${CORE_SOURCES} tests/p2/test_p2.cpp)
