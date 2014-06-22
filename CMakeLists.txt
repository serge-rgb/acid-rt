#===============================================================================
# nuwen software
#===============================================================================
cmake_minimum_required(VERSION 2.8)

add_subdirectory(third_party/lua)

project(nuwen)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

set(CMAKE_CXX_FLAGS "--std=c++11 -m64 -Weverything -Werror -Wno-c++98-compat -Wno-missing-prototypes -Wno-padded -fno-exceptions -Wno-old-style-cast -Wno-missing-noreturn -pthread")

set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DPH_DEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g -DNDEBUG")

#===============================================================================
# nuwen runtime
#===============================================================================

include_directories(
    "pham/"
    "./third_party/lua/"
    "./third_party/lua/src/"
    "./third_party/bdwgc/include/"
    )

add_library(gc STATIC IMPORTED)
set_property(TARGET gc PROPERTY IMPORTED_LOCATION
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib/libgc.a")

add_library(ph pham/ph.cc)
set(nuwen_runtime liblua gc ph)

#===============================================================================
# Projects
#===============================================================================

# ---- Budget manager
# add_subdirectory(budget)
