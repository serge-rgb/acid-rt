// 2014 Sergio Gonzalez

#include <ph.h>

using namespace ph;

static GLuint g_program;

void draw() {
    {
        GLCHK ( glUseProgram(g_program) );
        GLCHK ( glDispatchCompute(512/8, 512/8, 1) );
        GLCHK ( glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT) );
    }
    vr::draw();
}
int main() {
    ph::init();
    int size[] = {512, 512};
    window::init("Test", size[0], size[1], window::InitFlag_default);

    const char* path = "test/compute.glsl";

    g_program = vr::init(size[0], size[1], &path, 1);

    window::draw_loop(draw);

    window::deinit();
}

