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
    cs::fill_screen();
}

int main() {
    ph::init();
    int size[] = {512, 512};
    window::init("Test", size[0], size[1], window::InitFlag_Default);

    const char* path = "test/compute.glsl";

    g_program = cs::init(size[0], size[1], &path, 1);

    window::main_loop(draw);

    window::deinit();
}

