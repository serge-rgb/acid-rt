// 2014 - Sergio Gonzalez

#pragma once

#include <system_includes.h>

namespace ph {
void query_gl_error(const char* expr, const char* file, int line);

////////////////////////////////////////
// Wrap gl functions
////////////////////////////////////////

#ifdef PH_DEBUG
#define GLCHK(stmt) \
    stmt; ph::query_gl_error(#stmt, __FILE__, __LINE__)
#else
#define GLCHK(stmt) stmt
#endif

inline void query_gl_error(const char* expr, const char* file, int line) {
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
        case GL_STACK_OVERFLOW:
            str = "GL_STACK_OVERFLOW";
            break;
        case GL_STACK_UNDERFLOW:
            str = "GL_STACK_UNDERFLOW";
            break;
        default:
            str = "SOME GL ERROR";
        }
        printf("%s in: %s:%d\n", str, file, line);
        printf("   ---- Expression: %s\n", expr);
    }
}
}  // ns ph
