// 2014 Sergio Gonzalez

#include <ph.h>

#include "mesh.h"
#include "ocl.h"
#include "ph_gl.h"
#include "scene.h"
#include "vr.h"
#include "window.h"

#include "samples.h"

using namespace ph;

static void bunny_idle() {
    ocl::draw();
}

void bunny_sample() {
    scene::init();

    auto big_chunk = mesh::load_obj(
            "third_party/ASSETS/bunny.obj", /*scale=*/10);

    auto chunks = mesh::shatter(big_chunk, 3);
    for (int i = 0; i < count(chunks); ++i) {
        // Bunny model appears to have the normals flipped.
        scene::submit_primitive(&chunks[i], scene::SubmitFlags_None);
    }

    scene::update_structure();
    scene::upload_everything();

    /* const char* fnames [6] = { */
    /*     "samples/skybox/negx.jpg", */
    /*     "samples/skybox/negy.jpg", */
    /*     "samples/skybox/negz.jpg", */
    /*     "samples/skybox/posx.jpg", */
    /*     "samples/skybox/posy.jpg", */
    /*     "samples/skybox/posz.jpg", */
    /* }; */

    /* static GLuint cube = 0; */
    /* // Ray tracer automaticlly looks at GL_TEXTURE2 for a skybox. */
    /* if (!cube) { */
    /*     cube = gl::create_cubemap(GL_TEXTURE2, fnames); */
    /* } */
    /* vr::enable_skybox(); */
    io::set_wasd_camera(-0.4f, 1, 2);

    { // Release big chunk
       phree(big_chunk.verts);
       phree(big_chunk.norms);
    }
    // Chunks are in heaven now.
    for (int i = 0; i < count(chunks); ++i) {
           phree(chunks[i].verts);
           phree(chunks[i].norms);
    }
    release(&chunks);

    window::main_loop(bunny_idle);
}
