// 2014 Sergio Gonzalez

#pragma once

#include "ph_base.h"

namespace ph {
namespace window {

extern GLFWwindow* m_window;

enum InitFlag {
    InitFlag_Default             = 1 << 0,
    InitFlag_NoDecoration        = 1 << 1,
    InitFlag_OverrideKeyCallback = 1 << 2,
};

typedef void (*WindowProc)();

void init(const char* title, int width, int height, InitFlag flags);

void main_loop(WindowProc draw_func);

void swap_buffers();

void deinit();

}  //ns window
}  //ns ph

