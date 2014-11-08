#pragma once

#include <ph.h>

////////////////////////////////////////////////
// GL IS DEPRECATED
////////////////////////////////////////////////

#define GL_DEPRECATED

////////////////////////////////////////////////

namespace ph {
namespace vr {

enum {
    EYE_Left,
    EYE_Right,
    EYE_Count,  // Works with most humanoid species
};

// Use get_hmd_constants() after init to get this:
// The minimum info needed to set up our renderer.
// TODO: Include distortion spline factors instead of hard-coding.
struct HMDConsts {
    float lens_centers[2][2];   // Left, Right
    float eye_to_screen;        // Will get you the optimal FOV when ray tracing.
    float viewport_size_m[2];     // The physical size of the rift display.
};

// State of eye queried at a point in time.
struct Eye {
    float orientation[4];
    float position[3];
    float pad;
};

extern ovrHmd  m_hmd;  // 'extern' because abstraction may leak when we try to use direct mode.

void init();
#ifndef GL_DEPRECATED
void init(int width, int height);
void init_with_shaders(int width, int height, const char** shader_paths, int num_shaders);
void draw(int* resolution);
#endif


const HMDConsts get_hmd_constants();

/**
 * The reason this is not simply GetMeAnEyeKPlzThx() is that LibOVR does much of its prediction with
 * `frame_index`, the time it takes us to draw a frame, and VSync time. So this needs to be followed
 * by a vr::end_frame() after we have done our thang.
 */
void begin_frame(Eye* left, Eye* right);
void end_frame();


void enable_skybox();

void disable_skybox();

void toggle_postproc();

void toggle_interlace_throttle();

void deinit();

}  // ns vr
}  // ns ph
