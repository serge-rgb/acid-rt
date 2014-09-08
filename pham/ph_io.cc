#include "ph_io.h"

#include "ph_base.h"


namespace ph
{
namespace io
{

static float wasd_step = -1.0f;
static float wasd_camera[3];

int wasd_pressed = 0;  // Bitmask for GLFW callback

void set_wasd_step(float step) {
    wasd_step = step;
}

const char* slurp(const char* path) {
    FILE* fd = fopen(path, "r");
    ph_expect(fd);
    if (!fd) {
        fprintf(stderr, "ERROR: couldn't slurp %s\n", path);
        ph::quit(EXIT_FAILURE);
    }
    fseek(fd, 0, SEEK_END);
    size_t len = (size_t)ftell(fd);
    fseek(fd, 0, SEEK_SET);
    const char* contents = phanaged(char, len);
    fread((void*)contents, len, 1, fd);
    return contents;
}

void get_wasd_camera(const float* quat, float* out_xyz) {
    auto glm_q = glm::quat(quat[0], quat[1], quat[2], quat[3]);
    auto axis = glm::axis(glm_q);
    auto e = glm::vec3(0, 0, 1);

    glm::vec3 rotated_e = e + 2.0f * glm::cross(glm::cross(e, axis) + glm_q.w * e, axis);

    GLfloat cam_step_z;
    GLfloat cam_step_x;

    cam_step_z = wasd_step * rotated_e.z;
    cam_step_x = wasd_step * rotated_e.x;

    if (io::wasd_pressed & io::Control_W) {
        wasd_camera[2] -= cam_step_z;
        wasd_camera[0] -= cam_step_x;
    }
    if (io::wasd_pressed & io::Control_S) {
        wasd_camera[2] += cam_step_z;
        wasd_camera[0] += cam_step_x;
    }

    e = glm::vec3(1, 0, 0);
    rotated_e = e + 2.0f * glm::cross(glm::cross(e, axis) + glm_q.w * e, axis);
    cam_step_z = wasd_step * rotated_e.z;
    cam_step_x = wasd_step * rotated_e.x;

    if (io::wasd_pressed & io::Control_A) {
        wasd_camera[2] += cam_step_z;
        wasd_camera[0] += cam_step_x;
    }
    if (io::wasd_pressed & io::Control_D) {
        wasd_camera[2] -= cam_step_z;
        wasd_camera[0] -= cam_step_x;
    }
    wasd_camera[1] = 0;
    out_xyz[0] = wasd_camera[0];
    out_xyz[1] = wasd_camera[1];
    out_xyz[2] = wasd_camera[2];
}

void wasd_callback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GL_TRUE);
    }
    if (key == GLFW_KEY_W && action == GLFW_PRESS) {
       wasd_pressed |= Control_W;
    }
    if (key == GLFW_KEY_A && action == GLFW_PRESS) {
       wasd_pressed |= Control_A;
    }
    if (key == GLFW_KEY_S && action == GLFW_PRESS) {
       wasd_pressed |= Control_S;
    }
    if (key == GLFW_KEY_D && action == GLFW_PRESS) {
       wasd_pressed |= Control_D;
    }
    if (key == GLFW_KEY_W && action == GLFW_RELEASE) {
       wasd_pressed &= ~Control_W;
    }
    if (key == GLFW_KEY_A && action == GLFW_RELEASE) {
       wasd_pressed &= ~Control_A;
    }
    if (key == GLFW_KEY_S && action == GLFW_RELEASE) {
       wasd_pressed &= ~Control_S;
    }
    if (key == GLFW_KEY_D && action == GLFW_RELEASE) {
       wasd_pressed &= ~Control_D;
    }
}

long get_microseconds() {
#ifdef _WIN32
    LARGE_INTEGER ticks;
    LARGE_INTEGER ticks_per_sec;
    QueryPerformanceFrequency(&ticks_per_sec);
    QueryPerformanceCounter(&ticks);

    ticks.QuadPart *= 1000000;  // Avoid precision loss;
    return (long)(ticks.QuadPart / ticks_per_sec.QuadPart);
#else
    struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);
    long ns = tp.tv_nsec;
    return ns * 1000;
#endif
}

}  // ns io
}  // ns ph

