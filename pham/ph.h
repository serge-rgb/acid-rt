#pragma once

#include "system_includes.h"

////////////////////////////////////////
//  ==== Settings ====
////////////////////////////////////////

// Slices are GC'd unless PH_SLICES_ARE_MANUAL is def'd
#ifndef PH_SLICES_ARE_MANUAL
#define PH_SLICES_ARE_GCD 1;
#endif

////////////////////////////////////////
// Allocator macros
////////////////////////////////////////

#define ph_assert(expr) \
    if (!(expr)) {\
        fprintf(stderr, "Failed assertion at %s, %d\n", __FILE__, __LINE__);\
        ph::quit(-1);\
    }

#define phanaged(type, num) \
        (type*)ph::memory::typeless_managed(sizeof(type) * num)

#define phanaged_realloc(type, old, num) \
    (type*)ph::memory::typeless_managed_realloc(old, sizeof(type) * num)

#define phalloc(type, num) \
    (type*)ph::memory::typeless_alloc(sizeof(type) * num)

#define phree(mem)\
        ph::memory::typeless_free((void*)mem); mem = NULL

namespace ph {

//////////////////////////
// Int Types
//////////////////////////

typedef int64_t int64;
typedef int32_t int32;

//////////////////////////
// Memory
//////////////////////////
namespace memory {
// In debug mode, query dynamic memory allocated.
int64 bytes_allocated();

void* typeless_managed(size_t n_bytes);
void* typeless_managed_realloc(void* old, size_t n_bytes);

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
// Error handling
/////////////////////////
void phatal_error(const char* message);

void quit(int code);

///////////////////////////////////
// ==== Inline templated constructs
///////////////////////////////////

//////////////////////////////
// Slices
//////////////////////////////

// === Slice
// Conforms:   release, append, count
// Operators:  []
template<typename T>
struct Slice {
    T* ptr;
    size_t n_elems;
    size_t n_capacity;
    const T& operator[](const int64 i) {
        ph_assert(i >= 0);
        ph_assert((size_t )i < n_elems);
        return ptr[i];
    }
};

// Create a Slice
template<typename T>
Slice<T> MakeSlice(size_t n_capacity) {
    Slice<T> slice;
    slice.n_capacity = n_capacity;
    slice.n_elems = 0;
#if !defined(PH_SLICES_ARE_GCD)
    slice.ptr = phalloc(T, n_capacity);
#else
    slice.ptr = phanaged(T, n_capacity);
#endif
    return slice;
}

template<typename T>
#if !defined(PH_SLICES_ARE_GCD)
void release(Slice<T>* s) {
    if (s->ptr) {
        phree(s->ptr);
    }
#else
void release(Slice<T>*) {
#endif
}

// Add an element to the end.
template<typename T>
void append(Slice<T>* slice, T elem) {
    if (slice->n_elems > 0 && slice->n_capacity == slice->n_elems) {
        slice->n_capacity *= 2;
#if !defined(PH_SLICES_ARE_GCD)
        T* new_mem = phalloc(T, slice->n_capacity);
        memcpy(new_mem, slice->ptr, sizeof(T) * slice->n_elems);
        phree(slice->ptr);
        slice->ptr = new_mem;
#else
        slice->ptr = phanaged_realloc(T, slice->ptr,
                sizeof(T) * slice->n_elems);
#endif
    }
    slice->ptr[slice->n_elems] = elem;
    slice->n_elems++;
}

template<typename T>
int64 count(Slice<T>* slice) {
    return (int64)slice->n_elems;
}

}  // ns memory

