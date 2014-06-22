#include <ph.h>

namespace ph {
#ifdef PH_DEBUG
struct AllocRecord {
    void*  ptr;
    size_t size;
};

static size_t g_total_memory = 0;

// TODO: change to stretchy buffer.
static AllocRecord g_alloc_records[1024];
static int g_alloc_i = 0;
#endif

lua_State* new_lua() {
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    return L;
}

void phatal_error(const char* message) {
    printf("%s\n", message);
    exit(-1);
}

lua_State* run_script(const char* path) {
    lua_State* L = new_lua();
    /* int err = luaL_loadfile(L, path); */
    int err = luaL_dofile(L, path);
    if (err) {
        if (L) {
            phree(L);
            fprintf(stderr, "Could not load file %s\n", lua_tostring(L, -1));
            phatal_error("Lua fatal error.");
        }
        return NULL;
    }
    return L;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-parameter"
static void register_bytes(size_t alloc) {
#ifdef PH_DEBUG
    g_total_memory += alloc;
#endif
}
#pragma clang diagnostic pop

void* typeless_alloc(size_t n_bytes) {
    void* ptr = malloc(n_bytes);
    if (!ptr) {
        ph::phatal_error("Allocation failed");
    }
#ifdef PH_DEBUG
    g_alloc_records[g_alloc_i] = {ptr, n_bytes};
    g_alloc_i++;
    register_bytes(n_bytes);
#endif
    return ptr;
}

void typeless_free(void* mem) {
#ifdef PH_DEBUG
    for (int i = 0; i < 1024; ++i) {
        if (g_alloc_records[i].ptr == mem) {
            g_total_memory -= g_alloc_records[i].size;
            return;
        }
    }
#endif
    free(mem);
}

size_t bytes_allocated() {
#ifdef PH_DEBUG
    return g_total_memory;
#else
    return 0;
#endif
}

void quit(int code) {
    exit(code);
}


}

