cmake_minimum_required(VERSION 3.28)
project(chiasmos VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # for clangd
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(CHIASMOS_TESTS "Build the unit tests" ON)
option(CHIASMOS_SERVER "Build `chiasmos serve` (needs Boost JSON and fetches beauty)" ON)

# dependencies:
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(nlohmann_json 3.11 CONFIG REQUIRED)
find_path(OSMIUM_INC osmium/version.hpp REQUIRED)
find_path(PROTOZERO_INC protozero/version.hpp REQUIRED)
if(CHIASMOS_SERVER)
  set(Boost_USE_STATIC_LIBS ON) # keep the release tarball free of libboost_json.so
  find_package(Boost 1.80 CONFIG REQUIRED COMPONENTS json)
  include(FetchContent)
  set(BEAUTY_ENABLE_OPENSSL OFF CACHE BOOL "" FORCE)
  set(BEAUTY_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(beauty
    GIT_REPOSITORY https://github.com/dfleury2/beauty.git
    GIT_TAG 58b3abfeea824a78d3a50e29696c48ae8780c3e2 # master after 1.0.8
    SYSTEM EXCLUDE_FROM_ALL)
  FetchContent_MakeAvailable(beauty)
  # prevent beauty _FORTIFY_SOURCE optimization warning from breaking O0 Werror dev build
  target_compile_options(beauty PRIVATE -Wno-error=cpp)
endif()

add_library(chiasmos_osmium INTERFACE)
target_include_directories(chiasmos_osmium SYSTEM INTERFACE ${OSMIUM_INC} ${PROTOZERO_INC})
target_link_libraries(chiasmos_osmium INTERFACE ZLIB::ZLIB Threads::Threads)

add_library(chiasmos_flags INTERFACE)
target_compile_options(chiasmos_flags INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion)
target_include_directories(chiasmos_flags INTERFACE ${CMAKE_SOURCE_DIR}/src)

add_subdirectory(src)

if(CHIASMOS_TESTS)
  enable_testing()
  find_package(GTest CONFIG REQUIRED)
  add_subdirectory(tests)
endif()
