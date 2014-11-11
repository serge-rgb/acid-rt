#pragma once

namespace ph {
namespace scene {
    struct CLtriangle;
}
namespace ocl {


void init();
// Set triangle soup to be buffer.
void set_triangle_soup(scene::CLtriangle* tris, scene::CLtriangle* norms, size_t num_tris);
void idle();  // Rename to draw
void deinit();
}
}
