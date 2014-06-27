#include "ph_gl.h"

#include <ph.h>

namespace ph {
namespace gl {

GLuint compile_shader(const char* path, GLuint type) {
    GLuint obj = glCreateShader(type);
    GLCHK();
    const char* src = ph::io::slurp(path);
    GLCHK ( glShaderSource(obj, 1, &src, NULL) );
    GLCHK ( glCompileShader(obj) );
    // ERROR CHECKING
#ifdef PH_DEBUG
    int res = 0;
    GLCHK ( glGetShaderiv(obj, GL_COMPILE_STATUS, &res) );
    if (!res) {
        printf("%s\n", src);
        GLint length;
        GLCHK ( glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &length) );
        char* log = phalloc(char, length);
        GLsizei written_len;
        GLCHK ( glGetShaderInfoLog(obj, length, &written_len, log) );
        printf("Shader compilation failed. \n    ---- Info log:\n%s", log);
        ph_assert(false);
    } else {
        printf("INFO: Compiled shader: %s\n", path);
    }
#endif
    return obj;
}
void link_program(GLuint obj, GLuint shaders[], int64 num_shaders) {
    ph_assert(glIsProgram(obj));
    for (int i = 0; i < num_shaders; ++i) {
        ph_assert(glIsShader(shaders[i]));
        GLCHK ( glAttachShader(obj, shaders[i]) );
    }
    GLCHK ( glLinkProgram(obj) );

    // ERROR CHECKING
#ifdef PH_DEBUG
    int res = 0;
    GLCHK ( glGetProgramiv(obj, GL_LINK_STATUS, &res) );
    if (!res) {
        fprintf(stderr, "ERROR: program did not link.\n");
    } else {
        printf("INFO: Linked program %u\n", obj);
    }
    GLCHK ( glValidateProgram(obj) );
#endif
}

inline void query_error(const char* expr, const char* file, int line) {
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
        fprintf(stderr, "%s in: %s:%d\n", str, file, line);
        fprintf(stderr, "   ---- Expression: %s\n", expr);
    }
}
}  // ns gl
}  // ns ph
