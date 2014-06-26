// 2014 - Sergio Gonzalez

#pragma once

#include <system_includes.h>

namespace ph {
void query_gl_error(const char* file, int line);

#ifdef PH_DEBUG
#define QUERY_ERROR ph::query_gl_error(__FILE__, __LINE__)
#else
#define QUERY_ERROR
#endif

////////////////////////////////////////
// Wrap gl functions
////////////////////////////////////////
#define pglClear(...) glClear(__VA_ARGS__); QUERY_ERROR
#define pglFinish(...) glFinish(__VA_ARGS__); QUERY_ERROR

inline void query_gl_error(const char* file, int line) {
    GLenum err = glGetError();
    const char* str = "";
    if (err != GL_NO_ERROR) {
        switch(err) {
        case GL_INVALID_ENUM:
            str = "GL_INVALID_ENUM";
            break;
        case GL_INVALID_VALUE:
            str = "GL_INVALID_VALUE";
            break;
        case GL_INVALID_OPERATION:
            str = "GL_INVALID_OPERATION";
            break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            str = "GL_INVALID_FRAMEBUFFER_OPERATION";
            break;
        case GL_OUT_OF_MEMORY:
            str = "GL_OUT_OF_MEMORY";
            break;
        }
        printf("%s in: %s:%d\n", str, file, line);
    }
}
}  // ns ph
