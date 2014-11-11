// 2014 Sergio Gonzalez

#include <ph.h>
#include <ocl.h>
#include <scene.h>
#include <vr.h>

#include "samples.h"
#include "window.h"

using namespace ph;

static void cubes_idle() {
    //vr::draw(g_resolution);  // defined in samples.cc
    ocl::idle();
}

void cubes_sample() {
    ocl::init();
    scene::init();

    // Create test grid of cubes
    scene::Cube thing;
    {
        int x = 2;
        int y = 2;
        int z = 1;
        for (int i = 0; i < z; ++i) {
            for (int j = -4; j < y - 4; ++j) {
                for (int k = -x/2; k < x; ++k) {
                    thing = scene::make_cube(k * 1.1f, 4 + j * 1.1f, -5 - i*1.1f, 0.5);
                    scene::submit_primitive(&thing);
                }
            }
        }
        logf("INFO: Submitted %d polygons.\n", x * y * z * 12);
    }

    io::set_wasd_camera(0,0,0);

    scene::update_structure();

    scene::upload_everything();

    window::main_loop(cubes_idle);
}
