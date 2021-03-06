#pragma once

#include <ph.h>

struct GLFWwindow;

namespace ph
{
namespace io
{

// Returns the complete contents of file at path
const char* slurp(const char* path);

// ============ WASD control
enum
{
    Control_W     = 1 << 0,
    Control_A     = 1 << 1,
    Control_S     = 1 << 2,
    Control_D     = 1 << 3,
    Control_Space = 1 << 4,
    Control_C     = 1 << 5,
};
extern int wasd_pressed;

void set_wasd_camera(float x, float y, float z);

void set_wasd_step(float step);

void wasd_callback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/);

void get_wasd_camera(const float* orientation, float* out_xyz);
// ====================================

uint64_t get_microseconds();

}
}

