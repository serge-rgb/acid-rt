// 2014 Sergio Gonzalez

#pragma once

#include "ph_base.h"

namespace ph {
namespace window {

enum InitFlag {
    InitFlag_default,
    InitFlag_no_decoration = 1 << 0,
};

typedef void (*WindowProc)();

void init(const char* title, int width, int height, InitFlag flags);

void draw_loop(WindowProc func);

void deinit();

}  //ns window
}  //ns ph

