#include "ph_window.h"
#include "system_includes.h"
#include "ph_gl.h"

namespace ph {
namespace window {
GLFWwindow*  m_window = NULL;
static GLFWmonitor* m_rift_monitor;

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


void init(const char* title, int width, int height, InitFlag flags) {
    if (!glfwInit()) {
        ph::quit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, true);
    glfwWindowHint(GLFW_RESIZABLE, false);
    if (flags & InitFlag_NoDecoration) {
        glfwWindowHint(GLFW_DECORATED, false);
    }

////////////////////////////////////////
// Find rift
////////////////////////////////////////
    m_rift_monitor = NULL;
    int num_monitors;
    GLFWmonitor** monitors = glfwGetMonitors(&num_monitors);
    for (int i = 0; i < num_monitors; ++i) {
        GLFWmonitor* monitor = monitors[i];
        int w;
        int h;
        glfwGetMonitorPhysicalSize(monitor, &w, &h);
        if (w == 150 && h == 94) {  // Note: DK2 same size?
            m_rift_monitor = monitor;  // We just share a screen here. Set the position.
            break;
        }
    }


    // if m_rift_monitor is NULL, it will just open a window.
    // Else, it will try to be fullscreen.
    m_window = glfwCreateWindow(width, height, title, /*m_rift_monitor*/NULL, NULL);

    if (!m_window) {
        ph::quit(EXIT_FAILURE);
    }

    if (m_rift_monitor) {
        int x;
        int y;
        glfwGetMonitorPos(m_rift_monitor, &x, &y);
        glfwSetWindowPos(m_window, x, y);
    }

    int gl_version[] = {
        glfwGetWindowAttrib(m_window, GLFW_CONTEXT_VERSION_MAJOR),
        glfwGetWindowAttrib(m_window, GLFW_CONTEXT_VERSION_MINOR),
    };
    ph_assert(gl_version[0] == 4);
    ph_assert(gl_version[1] >= 3);
    printf("GL version is %d.%d\n", gl_version[0], gl_version[1]);

    glfwSetErrorCallback(error_callback);
    if (!(flags & InitFlag_OverrideKeyCallback)) {
        glfwSetKeyCallback(m_window, key_callback);
    }

    glfwMakeContextCurrent(m_window);

#ifdef _WIN32
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if( err != GLEW_OK ) {
        phatal_error("Couldn't init glew");
    }
    printf("Using GLEW %s\n", glewGetString(GLEW_VERSION));
#endif


}

void draw_loop(WindowProc func) {
    //=========================================
    // Main loop.
    //=========================================
    double total_time_ms = 0;
    int64 num_frames = 1;
    double ms_per_frame = 13.3333;
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();

        // ---- Get start time
        long start_ns = ph::io::get_microseconds();

        // Call user supplied draw function.
        {
            func();
        }

        // ---- Get end time. Measure
        GLCHK ( glFinish() );
        long diff = ph::io::get_microseconds() - start_ns;
        double diff_ms = double(diff) / (1000.0);
        if (diff_ms >= ms_per_frame) {
            fprintf(stderr, "Overshot: %fms\n", diff_ms);
        } else {
            if (diff_ms > 0) {
                total_time_ms += diff_ms;
                num_frames++;
                printf("Frame time: %fms\n", diff_ms);
            }
        }

        // ---- Swap
        glfwSwapBuffers(m_window);
    }

    double avg = total_time_ms / double(num_frames);
    printf("Average frame time in ms: %f  %f%%\n",
            avg, 100 * (avg/13.333));
}

void deinit() {
    glfwDestroyWindow(m_window);
}

}  //ns window
}  //ns ph

