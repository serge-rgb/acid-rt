#===============================================================================
# nuwen software
#===============================================================================
cmake_minimum_required(VERSION 2.8)

add_subdirectory(third_party/lua)
add_subdirectory(third_party/glfw)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "..")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "..")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "..")
add_subdirectory(third_party/bdwgc)
add_subdirectory(third_party/yajl)

project(nuwen)
if (UNIX)
    set(CMAKE_C_COMPILER clang)
    set(CMAKE_CXX_COMPILER clang++)

    set(CMAKE_CXX_FLAGS "--std=c++11 -m64 -Weverything -Werror -Wno-c++98-compat -Wno-missing-prototypes -Wno-padded -fno-exceptions -Wno-old-style-cast -Wno-missing-noreturn -Wno-c++98-compat-pedantic -pthread")

    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DPH_DEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g -DNDEBUG")
endif()
if (WIN32)
    set(CMAKE_CXX_FLAGS "/MP /Wall /WX /wd4514 /wd4505 /wd4996 /wd4756 /wd4056 /wd4738 /wd4686 /D_CRT_SECURE_NO_WARNINGS")
    set(CMAKE_CXX_FLAGS_DEBUG "/DPH_DEBUG /Od /ZI")
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /wd4711")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO ${CMAKE_CXX_FLAGS_RELEASE} " /Zi")

    set(CMAKE_EXE_LINKER_FLAGS "/SAFESEH:NO")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "/DEBUG")
    set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "/DEBUG")
    set(CMAKE_SHARED_LINKER_FLAGS "/SAFESEH:NO")
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "/DEBUG")
    set(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO "/DEBUG")
endif()

#===============================================================================
# nuwen runtime
#===============================================================================

find_package(OpenGL)

include_directories(
    "src/"

    "./third_party/lua/"
    "./third_party/lua/src/"
    "./third_party/bdwgc/include/"
    "./third_party/glfw/include/"
    "./third_party/yajl/src/api"
    "./third_party/glm/"
    "./third_party/stb/"

    "./OculusSDK/LibOVR/Include"
    )
if (WIN32)
    include_directories(
        "./Windows/"
        "./build/third_party/lua/"
        "./build/third_party/yajl/yajl-2.1.1/include"
        )
endif()

add_library(gc SHARED IMPORTED)
add_library(ovr STATIC IMPORTED)
if (UNIX)
    set_property(TARGET gc PROPERTY IMPORTED_LOCATION
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib/libgc.so")
    set_property(TARGET ovr PROPERTY IMPORTED_LOCATION
        "${CMAKE_CURRENT_SOURCE_DIR}/OculusSDK/LibOVR/Lib/Linux/Release/x86_64/libovr.a")
endif()
if (WIN32)
    set_property(TARGET ovr PROPERTY IMPORTED_LOCATION
        "${CMAKE_CURRENT_SOURCE_DIR}/OculusSDK/LibOVR/Lib/Win32/VS2013/libovr.lib")
    find_library(GLEW_LIB glew32 HINTS "./Windows/")
endif()


add_library(ph STATIC
    src/ph.cc
    )
target_link_libraries(ph gcmt-dll liblua glfw ${GLFW_LIBRARIES})
if (WIN32)
    target_link_libraries(ph ${GLEW_LIB} wsock32 ws2_32)
endif()
set(nuwen_runtime liblua gcmt-dll glfw ph ${OPENGL_LIBRARIES} yajl)

set (renderer_sources
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mesh.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/io.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ph_gl.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scene.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vr.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/window.cc
    )
#===============================================================================
# Projects
#===============================================================================

# ---- tests
add_subdirectory(chicken)
add_subdirectory(ph_test)
add_subdirectory(samples)
