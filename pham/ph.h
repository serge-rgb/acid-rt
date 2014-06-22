#pragma once

#include "system_includes.h"


#define ph_assert(expr) \
    if (!(expr)) {\
        fprintf(stderr, "Failed assertion at %s, %d\n", __FILE__, __LINE__);\
        ph::quit(-1);\
    }

#define phanage   GC_malloc

#define phalloc(type, num) (type*)ph::memory::typeless_alloc(sizeof(type) * num)

#define phree(mem) ph::memory::typeless_free((void*)mem); mem = NULL

namespace ph {

//////////////////////////
// Memory
//////////////////////////
namespace memory {
// In debug mode, query dynamic memory allocated.
size_t bytes_allocated();

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
    const T& operator[](const size_t i) {
        ph_assert(i < n_elems);
        return ptr[i];
    }
};

// Create a Slice
template<typename T>
Slice<T> MakeSlice(size_t n_capacity) {
    Slice<T> slice;
    slice.n_capacity = n_capacity;
    slice.n_elems = 0;
    slice.ptr = phalloc(T, n_capacity);
    return slice;
}

template<typename T>
void release(Slice<T>* s) {
    if (s->ptr) {
        phree(s->ptr);
    }
}

// Add an element to the end.
template<typename T>
void append(Slice<T>* slice, T elem) {
    if (slice->n_elems > 0 && slice->n_capacity == slice->n_elems) {
        slice->n_capacity *= 2;
        T* new_mem = phalloc(T, slice->n_capacity);
        memcpy(new_mem, slice->ptr, sizeof(T) * slice->n_elems);
        phree(slice->ptr);
        slice->ptr = new_mem;
    }
    slice->ptr[slice->n_elems] = elem;
    slice->n_elems++;
}

template<typename T>
size_t count(Slice<T>* slice) {
    return slice->n_elems;
}

}  // ns memory

