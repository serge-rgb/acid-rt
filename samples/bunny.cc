// 2014 Sergio Gonzalez

#include <ph.h>

#include "mesh.h"
#include "ph_gl.h"
#include "window.h"
#include "scene.h"
#include "vr.h"

#include "samples.h"

using namespace ph;

static void bunny_idle() {
    vr::draw(g_resolution);
}

void bunny_sample() {
    scene::init();

    auto big_chunk = mesh::load_obj_with_face_fmt(
            "third_party/bunny.obj", mesh::LoadFlags_NoTexcoords, /*scale=*/10);

    // 100 - 150 seems to be the sweetspot for bunny.obj
    auto chunks = mesh::shatter(big_chunk, 150);
    for (int i = 0; i < count(chunks); ++i) {
        // Bunny model appears to have the normals flipped.
        scene::submit_primitive(&chunks[i], scene::SubmitFlags_FlipNormals);
    }

    scene::update_structure();
    scene::upload_everything();

    const char* fnames [6] = {
        "samples/skybox/negx.jpg",
        "samples/skybox/negy.jpg",
        "samples/skybox/negz.jpg",
        "samples/skybox/posx.jpg",
        "samples/skybox/posy.jpg",
        "samples/skybox/posz.jpg",
    };

    static GLuint cube = 0;
    // Ray tracer automaticlly looks at GL_TEXTURE2 for a skybox.
    if (!cube) {
        cube = gl::create_cubemap(GL_TEXTURE2, fnames);
    }
    vr::enable_skybox();
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
