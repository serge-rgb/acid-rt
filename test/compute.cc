// Test for GLSL compute shaders.
#include <ph.h>


using namespace ph;

namespace test {

static GLuint m_texture;
static int m_size[2] = {1024, 768};
static GLuint m_program;
static GLuint m_quad_vao;
static GLuint vbo;

static GLuint m_compute_program;
static const int kWarpSize[2] = {8, 4};  // <--- WARNING: compile time def in CS shader.

// static GLuint m_compute_program;

void init() {
    enum Location {
        Location_pos,
        Location_tex,
    };

    // Create / fill texture
    {
        // Create texture
        GLCHK (glActiveTexture (GL_TEXTURE0) );
        GLCHK (glGenTextures   (1, &m_texture) );
        GLCHK (glBindTexture   (GL_TEXTURE_2D, m_texture) );

        // Note for the future: These are needed.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Pass a null pointer, texture will be filled by compute shader
        GLCHK ( glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, m_size[0], m_size[1],
                    0, GL_RGBA, GL_FLOAT, NULL) );

        // Bind it to image unit
        glBindImageTexture(0, m_texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    }
    // Create main program
    {
        enum {
            vert,
            frag,
        };
        GLuint shaders[2];
        const char* path[2];
        path[vert] = "glsl/quad.v.glsl";
        path[frag] = "glsl/quad.f.glsl";
        shaders[vert] = ph::gl::compile_shader(path[vert], GL_VERTEX_SHADER);
        shaders[frag] = ph::gl::compile_shader(path[frag], GL_FRAGMENT_SHADER);

        m_program = glCreateProgram();

        ph::gl::link_program(m_program, shaders, 2); GLCHK();

        ph_expect(Location_pos == glGetAttribLocation(m_program, "position"));
        ph_expect(Location_tex == glGetUniformLocation(m_program, "tex"));

        GLCHK ( glUseProgram(m_program) );
        glUniform1i(Location_tex, /*GL_TEXTURE_0*/0);

    }
    // Create a quad.
    {
        glPointSize(3);
        const GLfloat u = 1.0f;
        GLfloat vert_data[] = {
            -u, u,
            -u, -u,
            u, -u,
            u, u,
        };
        glGenVertexArrays(1, &m_quad_vao);
        glBindVertexArray(m_quad_vao);

        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);

        GLCHK (glBufferData (GL_ARRAY_BUFFER, sizeof (vert_data), vert_data, GL_STATIC_DRAW));

        GLCHK (glEnableVertexAttribArray (Location_pos) );
        GLCHK (glVertexAttribPointer     (/*attrib location*/Location_pos,
                    /*size*/2, GL_FLOAT, /*normalize*/GL_FALSE, /*stride*/0, /*ptr*/0));
    }
    // Create compute shader
    {
        GLuint shader = gl::compile_shader("test/compute.glsl", GL_COMPUTE_SHADER);

        m_compute_program = glCreateProgram();
        ph::gl::link_program(m_compute_program, &shader, 1);
        ph_expect(Location_tex == glGetUniformLocation(m_compute_program, "tex"));
        ph_expect(0 == glGetUniformLocation(m_compute_program, "screen_size"));
        glUseProgram(m_compute_program);
        GLCHK ( glUniform1i(Location_tex, 0) );  // Location: 0, Texture Unit: 0
        GLfloat size[2] = {(float)m_size[0], (float)m_size[1]};
        glUniform2fv(0, 1, &size[0]);
    }
}

void draw() {
    glClearColor(0.0, 0.5, 0.0, 1.0);
    GLCHK ( glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT) );

    // Dispatch CS
    {
        const GLuint num_wgrps_x = GLuint(m_size[0] / kWarpSize[0]);
        const GLuint num_wgrps_y = GLuint(m_size[1] / kWarpSize[1]);
        GLCHK ( glUseProgram(m_compute_program) );
        GLCHK ( glDispatchCompute(num_wgrps_x, num_wgrps_y, 1) );
        // Fence
        GLCHK ( glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT) );
    }

    // Draw screen quad
    {
        GLCHK (glUseProgram      (m_program) );
        GLCHK (glBindVertexArray (m_quad_vao) );
        GLCHK (glDrawArrays      (GL_TRIANGLE_FAN, 0, 4) );
    }
}
}  // ns test

int main() {
    ph::init();
    // This gets us a GL context, so we init it as soon as we can:
    ph::window::init("Compute test", 1024, 768, ph::window::InitFlag_default);

    test::init();

    ph::window::draw_loop(test::draw);

    ph::window::deinit();


    ph::quit(EXIT_SUCCESS);
}

