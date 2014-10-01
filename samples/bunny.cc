
// 2014 Sergio Gonzalez

#include <ph.h>
#include <scene.h>
#include <vr.h>

#include "samples.h"

using namespace ph;

static void bunny_idle() {
    vr::draw(g_resolution);
}

void bunny_sample() {
    scene::init();

    window::main_loop(bunny_idle);
}
