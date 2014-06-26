#pragma once

#pragma clang system_header
extern "C" {
    // LOCAL
#include <gc.h>
#define GL_GLEXT_PROTOTYPES
#include <GLFW/glfw3.h>
#include <lauxlib.h>
#include <lualib.h>
#include <lua.h>
    // SYSTEM
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
}
