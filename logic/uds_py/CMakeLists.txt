# Minimum CMake version required
cmake_minimum_required(VERSION 3.10)

set(prj_name "libuds")

# Project name and version
project(${prj_name} VERSION 1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS  "-ggdb -O0")

# Create the library
add_executable(
	${prj_name}
	STATIC
	${CMAKE_CURRENT_SOURCE_DIR}/output/uds_def.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/output/main.cpp
)

# Include directories
target_include_directories(${prj_name} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/output)

# Add any necessary compiler options or definitions
target_compile_options(${prj_name} PRIVATE -Wall -Wextra)