#pragma once

namespace ph {
namespace ocl {

struct CLTriangle {
    float p0[3];
    float pad0;
    float p1[3];
    float pad1;
    float p2[3];
    float pad2;
};

void init();
// Set triangle soup to be buffer.
void set_triangle_soup(CLTriangle* tris, CLTriangle* norms, int num_tris);
void idle();  // Rename to draw
void deinit();
}
}
