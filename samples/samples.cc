#include "sample_list.h"

#include <ph.h>
#include <vr.h>

#include "samples.h"
#include "sample_list.h"

using namespace ph;

static GLuint g_program;

int g_resolution[] = {1920, 1080};  // DK2 res

static int        g_curr_sample = 1;
static SampleFunc g_sample_func;

static void sample_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    ph::io::wasd_callback(window, key, scancode, action, mods);
    if (key == GLFW_KEY_Q && action == GLFW_PRESS) {
        vr::toggle_postproc();
    }
    if (key == GLFW_KEY_E && action == GLFW_PRESS) {
        vr::toggle_interlacing();
    }
    if (key == GLFW_KEY_LEFT && action == GLFW_PRESS) {
        g_curr_sample--;
        while(g_curr_sample < 0) g_curr_sample++;
        g_sample_func = g_samples[g_curr_sample];
        g_sample_func();
    }
    if (key == GLFW_KEY_RIGHT && action == GLFW_PRESS) {
        g_curr_sample++;
        while ((size_t)g_curr_sample >= g_num_samples) g_curr_sample--;
        g_sample_func = g_samples[g_curr_sample];
        g_sample_func();
    }
}

int main() {
    ph_assert(g_num_samples >= 1);
    ph::init();

    window::init("Samples", g_resolution[0], g_resolution[1],
                 window::InitFlag(
                 window::InitFlag_IgnoreRift |
                 window::InitFlag_NoDecoration | window::InitFlag_OverrideKeyCallback));


    io::set_wasd_step(0.06f);

    glfwSetKeyCallback(ph::window::m_window, sample_callback);

    vr::init(g_resolution[0], g_resolution[1]);

    g_sample_func = g_samples[g_curr_sample];

    g_sample_func();

    window::deinit();
    vr::deinit();
    printf("Done.\n");
}
