#include <ph.h>

#include <stdarg.h>


namespace ph {


void init() {
#ifndef PH_SLICES_ARE_MANUAL
    GC_init();
    GC_enable_incremental();
#endif
}

#if defined(PH_DEBUG)
void log(const char* s) {
    printf("%s\n", s);
#else
void log(const char*) {
#endif
}

#if defined(PH_DEBUG)
void logf(const char* s, ...) {
    va_list args;
    va_start(args, s);
    vfprintf(stdout, s, args);
#else
void logf(const char*, ...) {
#endif
}

void phatal_error(const char* message) {
    printf("%s\n", message);
    exit(-1);
}

///////////////////////////////
// Hash functions
///////////////////////////////
uint64_t hash(int64 data) {
    uint64_t hash = 5381;
    while (data) {
        hash = (hash * 33) ^ data;
        data /= 2;
    }
    return hash;
}

uint64_t hash(const char* s) {
    uint64_t hash = 5381;
    while (*s != '\0') {
        hash = (hash * 33) ^ (uint64_t)(*s);
        ++s;
    }
    return hash;
}

namespace memory {

void* typeless_managed(size_t n_bytes) {
    return GC_malloc(n_bytes);
}

/* void* typeless_managed_realloc(void* old, size_t n_bytes) { */
/*     return GC_realloc(old, n_bytes); */
/* } */

void* typeless_alloc(size_t n_bytes) {
    void* ptr = malloc(n_bytes);
    if (!ptr) {
        ph::phatal_error("Allocation failed");
    }
    return ptr;
}

void typeless_free(void* mem) {
    free(mem);
}

}  // ns memory

void quit(int code) {
#ifdef PH_DEBUG
    // TODO: boehm bug? uncomment later
    // GC_gcollect();
#endif
    exit(code);
}

}  // ns ph

