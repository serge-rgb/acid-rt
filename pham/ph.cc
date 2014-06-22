#include <ph.h>

namespace ph {

lua_State* new_lua() {
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    return L;
}

void phatal_error(const char* message) {
    printf("%s\n", message);
    exit(-1);
}

lua_State* script(const char* path) {
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


}

