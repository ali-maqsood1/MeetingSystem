cmake_minimum_required(VERSION 3.10)
project(MeetingSystem)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")

# Find required packages
find_package(Threads REQUIRED)

# Help CMake locate Homebrew installations on macOS
if(APPLE)
    # Try common Homebrew prefixes
    list(APPEND CMAKE_PREFIX_PATH 
        /opt/homebrew
        /usr/local
    )
endif()

# Prefer Boost's CMake CONFIG package (CMP0167) and imported targets
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
# Boost 1.78+ may have header-only System; prefer headers without component lookup
find_package(Boost CONFIG REQUIRED)

# Enable header-only Boost.System to avoid needing a compiled boost_system library
add_definitions(-DBOOST_ERROR_CODE_HEADER_ONLY)

# OpenCV is optional in many setups; keep if available
find_package(OpenCV QUIET)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/storage
    ${CMAKE_SOURCE_DIR}/src/models
    ${CMAKE_SOURCE_DIR}/src/server
    ${CMAKE_SOURCE_DIR}/src/managers
    ${CMAKE_SOURCE_DIR}/src/utils
    ${Boost_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
)

# Storage library
add_library(storage
    src/storage/DatabaseEngine.cpp
    src/storage/BTree.cpp
    src/storage/HashTable.cpp
)

# Test executable for storage engine
add_executable(test_storage
    tests/test_storage.cpp
)

target_link_libraries(test_storage
    storage
    Threads::Threads
)

# Server library
add_library(server
    src/server/HTTPServer.cpp
)

target_link_libraries(server
    Threads::Threads
    ${Boost_LIBRARIES}
)

# Managers library
add_library(managers
    src/managers/AuthManager.cpp
    src/managers/MeetingManager.cpp
    src/managers/ChatManager.cpp
    src/managers/FileManager.cpp
    src/managers/WhiteboardManager.cpp
    src/managers/ScreenShareManager.cpp
    src/utils/Hash.cpp
)

target_link_libraries(managers
    storage
)

# Main server executable
add_executable(meeting_server
    src/main.cpp
)

target_link_libraries(meeting_server
    storage
    server
    managers
    Threads::Threads
    ${Boost_LIBRARIES}
)