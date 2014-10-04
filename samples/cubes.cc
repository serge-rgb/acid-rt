// 2014 Sergio Gonzalez

#include <ph.h>
#include <scene.h>
#include <vr.h>

#include "samples.h"

using namespace ph;

static void cubes_idle() {
    vr::draw(g_resolution);  // defined in samples.cc
}

void cubes_sample() {
    scene::init();

    // Create test grid of cubes
    scene::Cube thing;
    {
        int x = 8;
        int y = 8;
        int z = 8;
        for (int i = 0; i < z; ++i) {
            for (int j = -4; j < y - 4; ++j) {
                for (int k = -x/2; k < x; ++k) {
                    thing = scene::make_cube(k * 1.1f, j * 1.1f, -2 - i*1.1f, 0.5);
                    scene::submit_primitive(&thing);
                }
            }
        }
        logf("INFO: Submitted %d polygons.\n", x * y * z * 12);
    }

    scene::update_structure();
    scene::upload_everything();

    window::main_loop(cubes_idle);
}
