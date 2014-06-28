#include "ph_window.h"
#include "system_includes.h"
#include "ph_gl.h"

namespace ph {
namespace window {
static GLFWmonitor* m_monitor;
static GLFWwindow*  m_window;




////////////////////////////////////////
// Default event & error handlers
////////////////////////////////////////
static void error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

static void key_callback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GL_TRUE);
    }
}


void init(const char* title, int width, int height, InitFlag /*flags*/) {
    if (!glfwInit()) {
        ph::quit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, true);

    m_monitor = NULL;
    m_window = glfwCreateWindow(width, height, title, m_monitor, NULL);

    if (!m_window) {
        ph::quit(EXIT_FAILURE);
    }

    int gl_version[] = {
        glfwGetWindowAttrib(m_window, GLFW_CONTEXT_VERSION_MAJOR),
        glfwGetWindowAttrib(m_window, GLFW_CONTEXT_VERSION_MINOR),
    };
    ph_assert(gl_version[0] == 4);
    ph_assert(gl_version[1] >= 3);
    printf("GL version is %d.%d\n", gl_version[0], gl_version[1]);

    glfwSetErrorCallback(error_callback);
    glfwSetKeyCallback(m_window, key_callback);

    glfwMakeContextCurrent(m_window);

}

void draw_loop(WindowProc func) {
    //=========================================
    // Main loop.
    //=========================================
    int64 total_time_ms = 0;
    int64 num_frames = 0;
    int ms_per_frame = 16;
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();

        // ---- Get start time
        struct timespec tp;
        clock_gettime(CLOCK_REALTIME, &tp);
        long start_ns = tp.tv_nsec;

        // Call user supplied draw function.
        {
            func();
        }

        // ---- Get end time. Measure
        GLCHK ( glFinish() );  // Make GL finish.
        clock_gettime(CLOCK_REALTIME, &tp);
        long diff = tp.tv_nsec - start_ns;
        double diff_ms = double(diff) / (1000.0 * 1000.0);
        if (diff_ms >= ms_per_frame) {
            fprintf(stderr, "Overshot: %fms\n", diff_ms);
        } else {
            if (diff_ms > 0) {
                total_time_ms += diff_ms;
                num_frames++;
            }
        }

        // ---- Swap
        glfwSwapBuffers(m_window);
    }

    printf("Average frame time in ms: %f\n",
            float(total_time_ms) / float(num_frames));
}

void deinit() {
    glfwDestroyWindow(m_window);
}

}  //ns window
}  //ns ph

