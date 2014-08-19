#include <ph.h>

#include <scene.h>

using namespace ph;

static int g_resolution[2] = { 1920, 1080 };
static GLuint g_program = 0;

void chicken_draw() {
    window::swap_buffers();
}

int main() {
    ph::init();
    window::init("Chicken", g_resolution[0], g_resolution[1],
        window::InitFlag(window::InitFlag_NoDecoration | window::InitFlag_OverrideKeyCallback
            | window::InitFlag_IgnoreRift
            ));

    static const char* tracing = "pham/tracing.glsl";
    g_program = cs::init(g_resolution[0], g_resolution[1], &tracing, 1);

    glfwSetKeyCallback(ph::window::m_window, ph::io::wasd_callback);

    vr::init(g_program);
    ovrHmd_AttachToWindow(vr::m_hmd, window::m_window, NULL, NULL);

    scene::init();

    window::main_loop(chicken_draw);

    window::deinit();
    vr::deinit();
    return 0;
}
