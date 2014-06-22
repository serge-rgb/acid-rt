#pragma once

#include "system_includes.h"


#define ph_assert(expr) \
    if (expr) {\
        fprintf(stderr, "Failed assertion at %s, %s", __FILE__, __LINE__);\
        ph::exit(-1);\
    }

#define phanage   GC_malloc

#define phalloc(type, num) \
    (type*)ph::typeless_alloc(sizeof(type) * num);

#define phree(mem)\
    ph::typeless_free((void*)mem);

namespace ph {

//////////////////////////
// Memory
//////////////////////////
void* typeless_alloc(size_t n_bytes);
void typeless_free(void* mem);
size_t bytes_allocated();

/////////////////////////
// Lua
/////////////////////////
lua_State* run_script(const char* path);

/////////////////////////
// Error handling
/////////////////////////
void phatal_error(const char* message);

void quit(int code);
}

