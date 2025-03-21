cmake_minimum_required(VERSION 3.21...3.31)

project(mcpelauncherguiscale)

set(CMAKE_CXX_STANDARD 23)

set(CPM_SOURCE_CACHE ${PROJECT_SOURCE_DIR}/cpm-cache CACHE PATH "")
include(cmake/CPM.cmake)

add_library(mcpelauncherguiscale SHARED
        src/main.cpp
        src/menu.hpp)

target_compile_options(mcpelauncherguiscale PRIVATE -Wall -Wextra -Wconversion -Wpedantic)

CPMAddPackage("gh:BasedInc/libhat@0.4.0")
CPMAddPackage("gh:stephenberry/glaze@5.0.1")
target_link_libraries(mcpelauncherguiscale PRIVATE libhat::libhat glaze::glaze)

install(TARGETS mcpelauncherguiscale LIBRARY DESTINATION mods)
