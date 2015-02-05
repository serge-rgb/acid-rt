cmake_minimum_required(VERSION 2.8)

add_subdirectory(third_party/glfw)
add_subdirectory(third_party/bdwgc)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR})

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "..")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "..")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "..")

project(acid)

if (UNIX)
    set(CMAKE_C_COMPILER clang)
    set(CMAKE_CXX_COMPILER clang++)

    set(CMAKE_CXX_FLAGS "--std=c++11 -m64 -Weverything -Werror -Wno-c++98-compat -Wno-missing-prototypes -Wno-padded -fno-exceptions -Wno-old-style-cast -Wno-missing-noreturn -Wno-c++98-compat-pedantic -pthread")

    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DPH_DEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g -DNDEBUG")
endif()
if (WIN32)
    set(CMAKE_CXX_FLAGS "/MP /Wall /WX /wd4514 /wd4711 /wd4505 /wd4996 /wd4756 /wd4056 /wd4738 /wd4686 /wd4820 /D_CRT_SECURE_NO_WARNINGS ")
    set(CMAKE_CXX_FLAGS_DEBUG "/DPH_DEBUG /Od /Zi ")
    set(CMAKE_CXX_FLAGS_RELEASE " /O2 ")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO " /Zi ")

    set(CMAKE_EXE_LINKER_FLAGS "/SAFESEH:NO")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "/DEBUG")
    set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "/DEBUG")
    set(CMAKE_SHARED_LINKER_FLAGS "/SAFESEH:NO")
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "/DEBUG")
    set(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO "/DEBUG")
endif()

find_package(OpenGL REQUIRED)

if (WIN32)
    set(OPENCL_INCLUDE_DIR "third_party/CL")

    find_package(OpenCL)

    if(OPENCL_FOUND)
        set(OPENCL_LIBRARIES ${OPENCL_LIBRARY})
        message("Found OpenCL ${OPENCL_LIBRARY}")
    else(OPENCL_FOUND)
        set(OPENCL_LIBRARIES)
        message("OPENCL NOT FOUND")
    endif(OPENCL_FOUND)
else()
    find_package(OpenCL REQUIRED)
endif()

include_directories(
    "src/"

    "./third_party/"
    "./third_party/glfw/include/"
    "./third_party/glm/"
    "./third_party/stb/"

    "./OculusSDK/LibOVR/Include"
    )
if (WIN32)
    include_directories(
        "./Windows/"
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
        "${CMAKE_CURRENT_SOURCE_DIR}/OculusSDK/LibOVR/Lib/x64/VS2013/libovr64.lib")
    find_library(GLEW_LIB glew32 HINTS "./Windows/")
    set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} "third_party/")
endif()


add_library(ph STATIC ${CMAKE_CURRENT_SOURCE_DIR}/src/ph.cc)
target_link_libraries(ph  glfw ${GLFW_LIBRARIES})
if (WIN32)
    target_link_libraries(ph ${GLEW_LIB} wsock32 ws2_32)
endif()
set(acid_runtime glfw ph ${OPENGL_LIBRARIES} ${OPENCL_LIBRARY})

set (renderer_sources
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mesh.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/io.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ocl.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ph_gl.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scene.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vr.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/window.cc
    )
#===============================================================================
# Projects
#===============================================================================

# ---- tests
add_subdirectory(ph_test)
add_subdirectory(samples)
