// 2014 Sergio Gonzalez

#include <ph.h>

using namespace ph;

static GLuint g_program;
static int g_size[] = {1920, 1080};
static int g_warpsize[] = {8, 8};

void draw() {
    // Dispatch ray tracing and fence.
    {
        GLCHK ( glUseProgram(g_program) );
        GLCHK ( glDispatchCompute(GLuint(g_size[0] / g_warpsize[0]),
                    GLuint(g_size[1] / g_warpsize[1]), 1) );
        GLCHK ( glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT) );
    }
    vr::draw();
}


int main() {
    ph::init();

    window::init("Project TARDIS", g_size[0], g_size[1], window::InitFlag_default);

    const char* paths[] = {
        "tardis/main.glsl",
    };

    g_program = vr::init(g_size[0], g_size[1], paths, 1);

    window::draw_loop(draw);

    window::deinit();
}

