#include "sample_list.h"

#include <ph.h>
#include <vr.h>

#include "samples.h"
#include "sample_list.h"

using namespace ph;

static GLuint g_program;

int g_resolution[] = {1920, 1080};  // DK2 res

static void sample_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    ph::io::wasd_callback(window, key, scancode, action, mods);
    if (key == GLFW_KEY_Q && action == GLFW_PRESS) {
        vr::toggle_postproc();
    }
}

int main() {
    ph_assert(g_num_samples >= 1);
    ph::init();

    window::init("Samples", g_resolution[0], g_resolution[1],
                 window::InitFlag(
                 /* window::InitFlag_IgnoreRift | */
                 window::InitFlag_NoDecoration | window::InitFlag_OverrideKeyCallback));


    io::set_wasd_step(0.06f);

    glfwSetKeyCallback(ph::window::m_window, sample_callback);

    vr::init(g_resolution[0], g_resolution[1]);

    ovrHmd_AttachToWindow(vr::m_hmd, window::m_window, NULL, NULL);

    SampleFunc sample = g_samples[0];

    sample();

    window::deinit();
    vr::deinit();
    printf("Done.\n");
}
