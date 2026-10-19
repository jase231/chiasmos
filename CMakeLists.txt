cmake_minimum_required(VERSION 3.28)
project(chiasmos VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # for clangd
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(CHIASMOS_TESTS "Build the unit tests" ON)
option(CHIASMOS_SERVER "Build `chiasmos serve` (needs Boost.Beast)" ON)

# dependencies:
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(nlohmann_json 3.11 CONFIG REQUIRED)
find_path(OSMIUM_INC osmium/version.hpp REQUIRED)
find_path(PROTOZERO_INC protozero/version.hpp REQUIRED)
if(CHIASMOS_SERVER)
  find_package(Boost 1.80 CONFIG REQUIRED)
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
