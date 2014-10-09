#pragma once

#include "system_includes.h"


////////////////////////////////////////
// Allocator macros
////////////////////////////////////////

#ifdef PH_DEBUG
#define ph_expect(expr) \
    if (!(expr)) {\
        fprintf(stderr, "Failed expectation at %s, %d\n", __FILE__, __LINE__);\
        fprintf(stderr, "    ---- " #expr "\n");\
    }
#else
#define ph_expect(expr)
#endif

#ifdef PH_DEBUG
#define ph_assert(expr) \
    if (!(expr)) {\
        fprintf(stderr, "Failed assertion at %s, %d\n", __FILE__, __LINE__);\
        fprintf(stderr, "    ---- " #expr "\n");\
        ph::quit(-1);\
    }
#else
#define ph_assert(expr)
#endif

#define phanaged(type, num) \
        (type*)ph::memory::typeless_managed(sizeof(type) * num)

#define phanaged_realloc(type, old, num) \
    (type*)ph::memory::typeless_managed_realloc(old, sizeof(type) * num)

#define phalloc(type, num) \
    (type*)ph::memory::typeless_alloc(size_t(num) * sizeof(type))

#define phree(mem)\
        ph::memory::typeless_free((void*)mem); mem = NULL

namespace ph {

void init();

//////////////////////////
// Int Types
//////////////////////////

typedef int64_t int64;
typedef int32_t int32;

#define PH_MAX_int32 2147483648
#define PH_MAX_int64 9223372036854775808

//////////////////////////
// Memory
//////////////////////////
namespace memory {
// In debug mode, query dynamic memory allocated.
int64 bytes_allocated();

void* typeless_managed(size_t n_bytes);
//////////// Note: Boehm has had bugs with this for years.
/* void* typeless_managed_realloc(void* old, size_t n_bytes); */

// malloc equivalent. Use phalloc macro
void* typeless_alloc(size_t n_bytes);

// free equivalent. Use phree macro
void typeless_free(void* mem);
}  // ns memory

/////////////////////////
// Lua
/////////////////////////
lua_State* run_script(const char* path);

/////////////////////////
// Logging
/////////////////////////

void log(const char* s);
void logf(const char* fmt, ...);

/////////////////////////

///////////////////////////////
// Hash functions
///////////////////////////////
uint64_t hash(int64 data);

uint64_t hash(const char* s);

// Error handling
/////////////////////////
void phatal_error(const char* message);

void quit(int code);

}  // ns ph

