#include <ph.h>

#include "mesh.h"
#include "ocl.h"
#include "vr.h"
#include "window.h"

#include "samples.h"

using namespace ph;

static void sponza_idle() {
    ocl::idle();
}

void sponza_sample() {
    ocl::init();
    scene::init();

    auto big_chunk = mesh::load_obj("third_party/ASSETS/sponza.obj", 0.02f);
    logf("Num verts in sponza: %ld\n", big_chunk.num_verts);
    auto small_chunks = mesh::shatter(big_chunk, 10);
    logf("Small chunks: %lu\n", count(small_chunks));
    for (int i = 0; i < count(small_chunks); ++i) {
        scene::submit_primitive(&small_chunks[i]);
    }

    vr::disable_skybox();
    vr::toggle_interlace_throttle();

    scene::update_structure();
    scene::upload_everything();

    window::main_loop(sponza_idle);
}
