# CMake minimum required: 3.15
cmake_minimum_required(VERSION 3.15)
# Project information
project(zps C)
# Target
set(TARGET "zps")
# Add project source
add_executable(${TARGET})
target_sources(${TARGET} PRIVATE src/${TARGET}.c src/${TARGET}.h)
# Compile options
target_compile_options(${TARGET} PRIVATE -s -O3 -Wall -Wextra -pedantic)
target_compile_definitions(${TARGET} PRIVATE NDEBUG)
# Install
install(TARGETS ${TARGET} RUNTIME DESTINATION bin)
