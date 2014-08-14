#pragma once

#include <ph.h>

namespace ph {
namespace vr {

extern float                     m_default_eye_z;     // Eye distance from plane.
extern ovrHmd                    m_hmd;
extern const OVR::HMDInfo*       m_hmdinfo;
extern const OVR::HmdRenderInfo* m_renderinfo;
extern float                     m_screen_size_m[2];  // Screen size in meters

void init(GLuint program);

void draw(int* resolution, int* warp_size);

void deinit();
}

namespace scene {

enum SubmitFlags {
    SubmitFlags_None = 1 << 0,
    SubmitFlags_FlipNormals
};

struct Cube {
    glm::vec3 center;
    glm::vec3 sizes;
    int index;          //  Place in triangle pool where associated triangles begin.
};

void init();

// ---- submit_primitive
// Add primitives to scene.

void submit_primitive(Cube* cube, SubmitFlags flags = SubmitFlags_None);

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
