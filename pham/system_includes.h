#pragma once

#pragma clang system_header

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#ifdef PH_OVR
#include <OVR.h>

// I need this to get a HMDInfo
#include "../OculusSDK/LibOVR/Src/CAPI/CAPI_HMDState.h"
typedef OVR::CAPI::HMDState hmdState;

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
    // SYSTEM
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
}

