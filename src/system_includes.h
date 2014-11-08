#pragma once

#if defined(_MSC_VER)

#pragma warning( push, 0 )

#else

#pragma clang system_header

#endif

// ==== glm
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// ==== Yajl
#include <yajl/yajl_tree.h>

// ==== stb
#include <stb_image.h>

#ifdef PH_OVR
#include <OVR.h>

// I need this to get a HMDInfo
#include "../OculusSDK/LibOVR/Src/CAPI/CAPI_HMDState.h"

#endif

extern "C" {
    // LOCAL
#include <gc.h>
#define GL_GLEXT_PROTOTYPES
#include <lauxlib.h>
#include <lualib.h>
#include <lua.h>
    // Cross platform
#ifdef _WIN32
#include <Windows.h>
#include <GL/glew.h>
#else
#include <unistd.h>
#endif
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#include <GLFW/glfw3native.h>
    // SYSTEM
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

