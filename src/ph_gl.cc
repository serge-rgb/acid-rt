#include "ph_gl.h"

#include <ph.h>

#if defined(_MSC_VER)

#pragma warning( push, 0 )

#endif

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include "io.h"


namespace ph
{
namespace gl
{


GLuint compile_shader(const char* path, GLuint type)
{
    GLuint obj = glCreateShader(type);
    const char* src = ph::io::slurp(path);
    GLCHK ( glShaderSource(obj, 1, &src, NULL) );
    GLCHK ( glCompileShader(obj) );
    // ERROR CHECKING
#ifdef PH_DEBUG
    int res = 0;
    GLCHK ( glGetShaderiv(obj, GL_COMPILE_STATUS, &res) );
    if (!res)
    {
        logf("%s\n", src);
        GLint length;
        GLCHK ( glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &length) );
        char* log = phalloc(char, length);
        GLsizei written_len;
        GLCHK ( glGetShaderInfoLog(obj, length, &written_len, log) );
        logf("Shader compilation failed. \n    ---- Info log:\n%s", log);
        phatal_error("Exiting: Error compiling shader");
    }
    else
    {
        logf("INFO: Compiled shader: %s\n", path);
    }
#endif
    return obj;
}
void link_program(GLuint obj, GLuint shaders[], int64 num_shaders)
{
    ph_assert(glIsProgram(obj));
    for (int i = 0; i < num_shaders; ++i)
    {
        ph_assert(glIsShader(shaders[i]));
        GLCHK ( glAttachShader(obj, shaders[i]) );
    }
    GLCHK ( glLinkProgram(obj) );

    // ERROR CHECKING
#ifdef PH_DEBUG
    int res = 0;
    GLCHK ( glGetProgramiv(obj, GL_LINK_STATUS, &res) );
    if (!res)
    {
        logf("ERROR: program did not link.\n");
        GLint len;
        glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &len);
        GLsizei written_len;
        char* log = phalloc(char, len);
        glGetProgramInfoLog(obj, (GLsizei)len, &written_len, log);
        logf("%s\n", log);
    }
    else
    {
        logf("INFO: Linked program %u\n", obj);
    }
    GLCHK ( glValidateProgram(obj) );
#endif
}

GLuint create_cubemap(GLuint texture_unit, const char* paths[6])
{
    GLuint tex;
    GLCHK ( glActiveTexture(texture_unit) );
    GLCHK ( glGenTextures(1, &tex) );
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    static const GLenum directions[6] =
    {
        GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
        GL_TEXTURE_CUBE_MAP_POSITIVE_X,
        GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
        GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
    };

    uint8_t* image_data[6];
    for(int i = 0; i < 6; ++i)
    {
        int w, h;
        const char* fname = paths[i];
        image_data[i] = stbi_load(fname, (int*)&w, (int*)&h, NULL, 4);
        ph_assert(image_data[i]);

        GLCHK ( glTexImage2D(directions[i], 0, GL_RGBA,
                    w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, &(image_data[i])[0]) );

        stbi_image_free(image_data[i]);
    }
    return tex;
}

inline void query_error(const char* expr, const char* file, int line)
{
    GLenum err = glGetError();
    const char* str = "";
    if (err != GL_NO_ERROR)
    {
        switch(err)
        {
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
