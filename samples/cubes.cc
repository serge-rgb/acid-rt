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


#if 0
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


    vr::disable_skybox();
#endif

    {
        ocl::CLTriangle tri;
        tri.p0[0] = 0;
        tri.p0[1] = 0;
        tri.p0[2] = -5;


        tri.p1[0] = 1;
        tri.p1[1] = 0;
        tri.p1[2] = -5;


        tri.p2[0] = 0;
        tri.p2[1] = 1;
        tri.p2[2] = -5;

        ph::ocl::set_triangle_soup(&tri, &tri, 1);

    }
    scene::Cube thing;
    {
        thing = scene::make_cube(0, 0, -5, 1.0f);
        scene::submit_primitive(&thing);
    }

    io::set_wasd_camera(0,0,0);

    scene::update_structure();
    scene::upload_everything();

    window::main_loop(cubes_idle);
}
