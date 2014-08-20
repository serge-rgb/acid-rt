#pragma once

#include <ph.h>

namespace ph {
namespace vr {

extern ovrHmd  m_hmd;

void init(GLuint program);

void draw(int* resolution);

void deinit();
}

namespace scene {

enum SubmitFlags {
    SubmitFlags_None        = 1 << 0,
    SubmitFlags_FlipNormals = 1 << 1,
    SubmitFlags_Update      = 1 << 2,
};

struct Cube {
    glm::vec3 center;
    glm::vec3 sizes;
    int index;          //  Place in triangle pool where associated triangles begin.
};

void init();

// ---- submit_primitive
// Add primitives to scene.

int64 submit_primitive(Cube* cube, SubmitFlags flags = SubmitFlags_None, int64 flag_params = 0);

// ----------------------

// ---- update_primitive
// TODO: implement this

// ----------------------

// ---- After submitting or updating, update the acceleration structure

// Build acceleration structure,
// then upload it to GPU.
void update_structure();

// ----------------------

// ---- Functions to upload scene info to GPU.

// Submit data about primitives to GPU.
void upload_everything();

// ----------------------

}
}
