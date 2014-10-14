// 2014 - Sergio Gonzalez

#pragma once

#include <ph.h>

////////////////////////////////////////
// Wrap gl functions
////////////////////////////////////////

#ifdef PH_DEBUG
#define GLCHK(stmt) \
    stmt; ph::gl::query_error(#stmt, __FILE__, __LINE__)
#else
#define GLCHK(stmt) stmt
#endif

namespace ph {
namespace gl {

// ==== Shaders ====

GLuint compile_shader(const char* path, GLuint type);

void link_program(GLuint program, GLuint shaders[], int64 num_shaders);

// ==== Skybox ====
GLuint create_cubemap(const char* paths[6]);

void query_error(const char* expr, const char* file, int line);

}  // ns gl
}  // ns ph
