// 2014 - Sergio Gonzalez

#pragma once

#include <ph.h>
#include "ph_slice_inl.h"

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

GLuint compile_shader(const char* path, GLuint type);
GLuint link_program(GLuint shaders[], int64 num_shaders);

void query_error(const char* expr, const char* file, int line);

}  // ns gl
}  // ns ph
