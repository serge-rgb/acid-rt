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
        int x = 16;
        int y = 32;
        int z = 20;
        for (int i = 0; i < z; ++i) {
            for (int j = 0; j < y; ++j) {
                for (int k = 0; k < x; ++k) {
                    thing = {{i * 1.1, j * 1.1, -2 - k * 1.1}, {0.5, 0.5, 0.5}, -1};
                    scene::submit_primitive(&thing);
                }
            }
        }
        printf("INFO: Submitted %d polygons.\n", x * y * z * 12);
    }

    scene::update_structure();
    scene::upload_everything();

    window::main_loop(cubes_idle);
}
