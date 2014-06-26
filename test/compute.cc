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

namespace test {
static GLuint m_texture;
void init() {
    // Create texture
    GLCHK ( glGenTextures(1, &m_texture) );
    // fill it
}

void draw() {
    // Draw texture
}
}

#define foo(bar) bar
int main() {
    foo(puts("wtf"));
    ph::init();

    if (!glfwInit()) {
        ph::quit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, true);

    GLFWmonitor* monitor = NULL;
    GLFWwindow* window = glfwCreateWindow(640, 480, "Checkers", monitor, NULL);

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

    test::init();

    //=========================================
    // Main loop.
    //=========================================
    while (!glfwWindowShouldClose(window)) {
        struct timespec tp;
        clock_gettime(CLOCK_REALTIME, &tp);
        long start_ns = tp.tv_nsec;

        // FRAME
        GLCHK ( glClear(GL_COLOR_BUFFER_BIT) );
        glfwPollEvents();
        GLCHK ( glFinish() );
        glfwSwapBuffers(window);
        clock_gettime(CLOCK_REALTIME, &tp);

        long diff = tp.tv_nsec - start_ns;
        printf("ms: %f\n", double(diff) / double(1000000));
        printf("diff: %ld\n", diff);
        uint32_t sleep_ns = uint32_t((16 * 1000000) - diff);
        printf("sleep: %ud\n", sleep_ns);
        usleep(sleep_ns / 1000);
    }

    glfwDestroyWindow(window);

    return 0;
}
