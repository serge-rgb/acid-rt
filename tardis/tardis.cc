// 2014 Sergio Gonzalez

#include <ph.h>

using namespace ph;

static GLuint g_program;
static int g_size[] = {1920, 1080};
// Note: perf is really sensitive about this. Runtime tweak?
static int g_warpsize[] = {8, 8};

void init(GLuint prog) {
    // Eye-to-lens
    glUseProgram(prog);
    glUniform1f(3, 2.0f);  // eye to lens.
}

void draw() {
    GLCHK ( glUseProgram(g_program) );
    GLfloat viewport_size[2] = {
        GLfloat (g_size[0]) / 2,
        GLfloat (g_size[1])
    };
    glUniform2fv(0, 1, viewport_size);

    static GLfloat sphere_y = 0.0f;
    static float step_var = 0.0;
    sphere_y = sinf(step_var);
    glUniform1f(4, sphere_y);
    step_var += 0.05;

    // Dispatch left viewport
    {
        glUniform1f(2, 0);  // x_offset
        GLCHK ( glDispatchCompute(GLuint(g_size[0] / g_warpsize[0]),
                    GLuint(g_size[1] / g_warpsize[1]), 1) );
    }
    // Dispatch right viewport
    {
        glUniform1f(2, 960.0);  // y_offset
        GLCHK ( glDispatchCompute(GLuint(g_size[0] / g_warpsize[0]),
                    GLuint(g_size[1] / g_warpsize[1]), 1) );
    }

    // Fence
    GLCHK ( glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT) );

    // Draw screen
    cs::fill_screen();
}

int main() {
    ph::init();

    window::init("Project TARDIS", g_size[0], g_size[1], window::InitFlag_default);

    const char* paths[] = {
        "tardis/main.glsl",
    };

    g_program = cs::init(g_size[0], g_size[1], paths, 1);

    init(g_program);

    window::draw_loop(draw);

    window::deinit();
}

