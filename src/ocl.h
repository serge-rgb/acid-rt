#pragma once

namespace ph {
struct CLtriangle;
struct Primitive;
struct BVHNode;

namespace ocl {


void init();
// Set triangle soup to be buffer.
void set_triangle_soup(ph::CLtriangle* tris, ph::CLtriangle* norms, size_t num_tris);
void set_primitive_array(ph::Primitive* prims, size_t num_prims);
void set_flat_bvh(ph::BVHNode* tree, size_t num_nodes);
void draw();
void deinit();
}
}
