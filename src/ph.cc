#include <ph.h>

#include <stdarg.h>


namespace ph {

void init() {
#ifndef PH_SLICES_ARE_MANUAL
    /* GC_enable_incremental(); */
    GC_init();
#endif
}

#ifdef PH_DEBUG
struct AllocRecord {
    void* ptr;
    size_t size;
};

static size_t g_total_memory = 0;

extern Slice<AllocRecord> g_alloc_records;
Slice<AllocRecord> g_alloc_records;
static bool g_init = false;
#endif

lua_State* new_lua() {
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    return L;
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

lua_State* run_script(const char* path) {
    lua_State* L = new_lua();
    int err = luaL_dofile(L, path);
    if (err) {
        if (L) {
            fprintf(stderr, "Could not load&run file %s\n", lua_tostring(L, -1));
            lua_close(L);  // not needed. Leaving it here for documentation: There is a close func
            phatal_error("Lua fatal error.");
        }
        return NULL;
    }
    return L;
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
#ifdef PH_DEBUG
    if (!g_init) {
        g_init = true;
        g_alloc_records.n_capacity = 1024;
        g_alloc_records.n_elems = 0;
        size_t sz = sizeof(AllocRecord) * 1024;
        g_alloc_records.ptr = (AllocRecord*)malloc(sz);
    }
    AllocRecord r = {ptr, n_bytes};
    append(&g_alloc_records, r);
    g_total_memory += n_bytes;
#endif
    return ptr;
}

void typeless_free(void* mem) {
#ifdef PH_DEBUG
    for (int64 i = 0; i < (int64)g_alloc_records.n_elems; ++i) {
        AllocRecord r = g_alloc_records[i];
        if (r.ptr == mem) {
            g_total_memory -= r.size;
            goto end;
        }
    }
    end:
#endif
    free(mem);
}  // ns memory

int64 bytes_allocated() {
#ifdef PH_DEBUG
    return (int64)g_total_memory;
#else
    return 0;
#endif
}
}  // ns memory

uint64_t hash(const char* s) {
    uint64_t hash = 5381;
    while (*s != '\0') {
        hash = (hash * 33) ^ (uint64_t)(*s);
        ++s;
    }
    return hash;
}

void quit(int code) {
#ifdef PH_DEBUG
    free(g_alloc_records.ptr);
    // TODO: boehm bug? uncomment later
    // GC_gcollect();
#endif
    exit(code);
}

}  // ns ph

