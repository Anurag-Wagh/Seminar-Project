cmake_minimum_required(VERSION 3.16)
project(opal_project C)

# Root CMake entrypoint for the OPAL project.
# Delegates the actual build configuration to ral/CMakeLists.txt.
include(${CMAKE_CURRENT_LIST_DIR}/ral/CMakeLists.txt)
