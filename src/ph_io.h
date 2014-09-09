#pragma once

#include <ph.h>

namespace ph {
namespace io {


// Returns the complete contents of file at path
const char* slurp(const char* path);

// ============ WASD control
enum {
    Control_W = 1 << 0,
    Control_A = 1 << 1,
    Control_S = 1 << 2,
    Control_D = 1 << 3,
};
extern int wasd_pressed;

void set_wasd_step(float step);

void wasd_callback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/);

void get_wasd_camera(const float* orientation, float* out_xyz);
// ====================================

long get_microseconds();

}
}

