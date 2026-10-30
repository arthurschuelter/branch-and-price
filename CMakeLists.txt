cmake_minimum_required(VERSION 3.16)

project(branch_and_price LANGUAGES CXX)

option(ENABLE_GUROBI "Build with Gurobi support" OFF)

add_executable(bp
    src/main.cpp
    src/core/branch_and_price.cpp
)

target_compile_features(bp PRIVATE cxx_std_17)
target_include_directories(bp PRIVATE src)

if(MSVC)
    target_compile_options(bp PRIVATE /W4)
else()
    target_compile_options(bp PRIVATE -Wall -Wextra)
endif()

if(ENABLE_GUROBI)
    set(GUROBI_HOME "$ENV{GUROBI_HOME}" CACHE PATH "Gurobi installation directory")
    set(GUROBI_VERSION "130" CACHE STRING "Gurobi library version suffix")

    if(NOT GUROBI_HOME)
        message(FATAL_ERROR "Set GUROBI_HOME to the Gurobi installation directory")
    endif()

    find_path(GUROBI_INCLUDE_DIR gurobi_c++.h
        HINTS "${GUROBI_HOME}/include"
        REQUIRED
    )
    find_library(GUROBI_CXX_LIBRARY gurobi_c++
        HINTS "${GUROBI_HOME}/lib"
        REQUIRED
    )
    find_library(GUROBI_LIBRARY "gurobi${GUROBI_VERSION}"
        HINTS "${GUROBI_HOME}/lib"
        REQUIRED
    )

    target_include_directories(bp PRIVATE "${GUROBI_INCLUDE_DIR}")
    target_link_libraries(bp PRIVATE "${GUROBI_CXX_LIBRARY}" "${GUROBI_LIBRARY}" m)
    set_property(TARGET bp APPEND PROPERTY BUILD_RPATH "${GUROBI_HOME}/lib")
endif()
