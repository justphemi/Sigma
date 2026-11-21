# Optional CMake build (./build.sh does the same thing without CMake).
cmake_minimum_required(VERSION 3.10)
project(Sigma CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Embed the C runtime into the compiler binary.
file(READ ${CMAKE_SOURCE_DIR}/runtime/sigma_runtime.h SIGMA_RT)
file(WRITE ${CMAKE_BINARY_DIR}/gen/runtime_embed.h
  "static const char* const SIGMA_RUNTIME = R\"SIGRT(${SIGMA_RT})SIGRT\";\n")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS runtime/sigma_runtime.h)

file(GLOB SIGMA_SOURCES ${CMAKE_SOURCE_DIR}/compiler/*.cpp)
add_executable(sig ${SIGMA_SOURCES})
target_include_directories(sig PRIVATE ${CMAKE_BINARY_DIR}/gen)
target_compile_options(sig PRIVATE -Wall -Wextra)

install(TARGETS sig DESTINATION bin)
