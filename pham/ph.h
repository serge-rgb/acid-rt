#pragma once

#include "system_includes.h"

#define phanage   GC_malloc
#define phalloc   malloc
#define phrealloc realloc
#define phree     free

namespace ph {

// Load a lua script.
lua_State* script(const char* path);

void phatal_error(const char* message);


}

