#===============================================================================
# nuwen software
#===============================================================================
cmake_minimum_required(VERSION 2.8)

add_subdirectory(third_party/lua)
add_subdirectory(third_party/glfw)

project(nuwen)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

set(CMAKE_CXX_FLAGS "--std=c++11 -m64 -Weverything -Werror -Wno-c++98-compat -Wno-missing-prototypes -Wno-padded -fno-exceptions -Wno-old-style-cast -Wno-missing-noreturn -Wno-c++98-compat-pedantic -pthread")

set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DPH_DEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g -DNDEBUG")

#===============================================================================
# nuwen runtime
#===============================================================================

find_package(OpenGL)

include_directories(
    "pham/"
    "./third_party/lua/"
    "./third_party/lua/src/"
    "./third_party/bdwgc/include/"
    "./third_party/glfw/include/"
    "./third_party/glm/"
    "./OculusSDK/LibOVR/Include"
    )

add_library(gc SHARED IMPORTED)
set_property(TARGET gc PROPERTY IMPORTED_LOCATION
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib/libgc.so")

# Note: Linux x86_64 only
add_library(ovr STATIC IMPORTED)
set_property(TARGET ovr PROPERTY IMPORTED_LOCATION
    "${CMAKE_CURRENT_SOURCE_DIR}/OculusSDK/LibOVR/Lib/Linux/Release/x86_64/libovr.a")

add_library(ph
    pham/ph.cc
    pham/ph_cs.cc
    pham/ph_gl.cc
    pham/ph_io.cc
    pham/ph_window.cc
    )
set(nuwen_runtime liblua gc ph glfw ${OPENGL_LIBRARIES})

#===============================================================================
# Projects
#===============================================================================

# ---- tests
add_subdirectory(test)
# --- project tardis
add_subdirectory(tardis)
# ---- budget app (top secret)
# add_subdirectory(budget)
