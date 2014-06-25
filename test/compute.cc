// Test for GLSL compute shaders.
#include <ph.h>

static void error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

static void key_callback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GL_TRUE);
    }
}

int main() {
    ph::init();
    if (!glfwInit()) {
        ph::quit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, true);

    GLFWmonitor* monitor = NULL;
    GLFWwindow* window = glfwCreateWindow(640, 480, "rrt", monitor, NULL);

    if (!window) {
        ph::quit(EXIT_FAILURE);
    }

    int gl_version[] = {
        glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MAJOR),
        glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MINOR),
    };
    ph_assert(gl_version[0] == 4);
    ph_assert(gl_version[1] >= 3);
    printf("GL version is %d.%d\n", gl_version[0], gl_version[1]);

    glfwSetErrorCallback(error_callback);
    glfwSetKeyCallback(window, key_callback);

    glfwMakeContextCurrent(window);

    //=========================================
    // Main loop.
    //=========================================
    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);
        glfwPollEvents();
        glfwSwapBuffers(window);
        usleep(16000);
    }

    glfwDestroyWindow(window);

    return 0;
}
