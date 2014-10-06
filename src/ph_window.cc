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
    if (!(flags & InitFlag_IgnoreRift)) {
        int num_monitors;
        GLFWmonitor** monitors = glfwGetMonitors(&num_monitors);
        for (int i = 0; i < num_monitors; ++i) {
            GLFWmonitor* monitor = monitors[i];
            auto* vidmode = glfwGetVideoMode(monitor);
            if (vidmode->refreshRate >= 75 &&
                    vidmode->width == 1920 &&
                    vidmode->height == 1080) {
                m_rift_monitor = monitor;
                break;
            }
        }
    }

    // if m_rift_monitor is NULL, it will just open a window.
    // Else, it will try to be fullscreen.
    m_window = glfwCreateWindow(width, height, title, m_rift_monitor, NULL);

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

    glGetError();  // glew generates an error. Not our fault
#endif
}

void main_loop(WindowProc step_func) {
    //=========================================
    // Main loop.
    //=========================================
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();

        // Call user supplied step function.
        {
            step_func();
        }
    }
}

void swap_buffers() {
    glfwSwapBuffers(m_window);
}

void deinit() {
    glfwDestroyWindow(m_window);
}

}  //ns window
}  //ns ph

