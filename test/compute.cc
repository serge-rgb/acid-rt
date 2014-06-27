// Test for GLSL compute shaders.
#include <ph.h>


using namespace ph;

static void error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

static void key_callback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GL_TRUE);
    }
}

namespace test {

static GLuint m_texture;
static int m_size = 512;
static GLuint m_program;
static GLuint m_quad_vao;

static GLuint m_compute_program;

// static GLuint m_compute_program;

void init() {
    enum Location {
        Location_pos,
        Location_tex,
    };

    // Create / fill texture
    {
        // Is this necessary?
        GLCHK ( glActiveTexture(GL_TEXTURE0) );
        // Create texture
        GLCHK ( glGenTextures(1, &m_texture) );
        GLCHK ( glBindTexture(GL_TEXTURE_2D, m_texture) );

        // Note for the future: These are needed.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // fill it
        float* data = phalloc(float, (size_t)(m_size * m_size * 4));
        for (int i = 0; i < m_size * m_size * 4; ++i) {
            data[i] = (i % 4 == 3) ? 1.0 : 0.5;
        }
        GLCHK ( glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_size, m_size, 0,
                    GL_RGBA, GL_FLOAT, (GLvoid*) data) );
        phree(data);
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

        // Bind locations before linking
        GLCHK ( glBindAttribLocation(m_program, Location_pos, "position") );

        ph::gl::link_program(m_program, shaders, 2); GLCHK();

        ph_assert(Location_pos == glGetAttribLocation(m_program, "position"));
        ph_assert(Location_tex == glGetUniformLocation(m_program, "tex"));

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

        GLuint vbo;
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        GLCHK (
                glBufferData(
                    GL_ARRAY_BUFFER,
                    sizeof(vert_data),
                    vert_data, GL_STATIC_DRAW)
                );

        GLCHK ( glEnableVertexAttribArray(Location_pos) );
        GLCHK(
            glVertexAttribPointer(/*attrib location*/Location_pos, /*size*/2, GL_FLOAT, /*normalize*/GL_FALSE,
                /*stride*/0, 0)
        );
    }
    // Create compute shader
    {
        GLuint shader = gl::compile_shader("test/compute.glsl", GL_COMPUTE_SHADER);
        m_compute_program = glCreateProgram();
        ph::gl::link_program(m_compute_program, &shader, 1);
    }
}

void draw() {
    glClearColor(0.5, 0.5, 0.5, 1.0);
    GLCHK ( glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT) );
    // Draw texture
    GLCHK ( glBindVertexArray(m_quad_vao) );
    GLCHK ( glDrawArrays(GL_TRIANGLE_FAN, 0, 4) );
}
}  // ns test

int main() {
    ph::init();

    if (!glfwInit()) {
        ph::quit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, true);

    GLFWmonitor* monitor = NULL;
    GLFWwindow* window = glfwCreateWindow(test::m_size, test::m_size, "Checkers", monitor, NULL);

    if (!window) {
        ph::quit(EXIT_FAILURE);
    }

    int gl_version[] = {
        glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MAJOR),
        glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MINOR),
    };
    ph_assert(gl_version[0] == 4);
    ph_assert(gl_version[1] >= 3);
    printf("GL version is %d.%d\n", gl_version[0], gl_version[1]);

    glfwSetErrorCallback(error_callback);
    glfwSetKeyCallback(window, key_callback);

    glfwMakeContextCurrent(window);

    test::init();

    //=========================================
    // Main loop.
    //=========================================
    int64 total_time_ms = 0;
    int64 num_frames = 0;
    int ms_per_frame = 16;
    while (!glfwWindowShouldClose(window)) {
        struct timespec tp;
        clock_gettime(CLOCK_REALTIME, &tp);
        long start_ns = tp.tv_nsec;

        glfwPollEvents();
        // FRAME
        test::draw();
        GLCHK ( glFinish() );
        glfwSwapBuffers(window);
        clock_gettime(CLOCK_REALTIME, &tp);

        num_frames++;
        long diff = tp.tv_nsec - start_ns;
        uint32_t sleep_ns = uint32_t((ms_per_frame * 1000000) - diff);
        auto sleep_us = sleep_ns/1000;
        if (sleep_us <= (uint)ms_per_frame * 1000) {
            usleep(sleep_us);
            total_time_ms += sleep_us / 1000;
        } else {
            printf("WARNING: Frame %ld overshot (in ms): %f\n",
                    num_frames, (sleep_us/1000) - double(ms_per_frame));
        }
    }

    printf("Average frame time in ms: %f\n",
            ms_per_frame - float(total_time_ms) / float(num_frames));

    glfwDestroyWindow(window);

    ph::quit(EXIT_SUCCESS);
}
