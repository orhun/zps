# CMake minimum required: 3.15
cmake_minimum_required(VERSION 3.15)
# Project information
project(zps C)
# Target
set(TARGET "zps")
# Add project source
add_executable(${TARGET} src/${TARGET}.c src/${TARGET}.h)
# Compile options
target_compile_options(${TARGET} PRIVATE -O3 -Wall)
# Install
install(TARGETS ${TARGET} RUNTIME DESTINATION bin)