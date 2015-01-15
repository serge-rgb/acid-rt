#pragma once

#include <ph.h>


namespace ph
{
namespace vr
{

enum
{
    EYE_Left,
    EYE_Right,
    EYE_Count,  // Works with most humanoid species
};

// Use get_hmd_constants() after init to get this:
// The minimum info needed to set up our renderer.
struct HMDConsts
{
    float lens_centers[2][2];       // Left, Right
    float eye_to_screen;            // Will get you the optimal FOV when ray tracing.
    float viewport_size_m[2];       // The physical size of the rift display.
    float meters_per_tan_angle;     // See OVR_Stereo.cpp : LoadLensConfig
};

// State of eye queried at a point in time.
struct Eye
{
    float orientation[4];
    float position[3];
    float pad;  // OpenCL alignment rules (CL 1.1 spec p.163)
};

struct RenderEyePose
{
    ovrPosef poses[2];
};

extern ovrHmd  m_hmd;  // 'extern' because abstraction may leak when we try to use direct mode.

void init();

const HMDConsts get_hmd_constants();

// The current (DK2) distortion correction uses a 11-point Catmull-Rom spline with
// t = radius_from_center_of_lens. This exposes the points for the current Rift relief.
void fill_catmull_K(float* K, int num_coefficients);

/**
 * The reason this is not simply GetMeAnEyeKPlzThx() is that LibOVR does much of its prediction with
 * `frame_index`, the time it takes us to draw a frame, and VSync time. So this needs to be followed
 * by a vr::end_frame() after we have done our thang.
 */
RenderEyePose begin_frame(Eye* left, Eye* right);
void end_frame(RenderEyePose* eye_pose, ovrMatrix4f twmatrices_l[2], ovrMatrix4f twmatrices_r[2]);


void enable_skybox();

void disable_skybox();

void toggle_postproc();

void toggle_interlace_throttle();

void deinit();

}  // ns vr
}  // ns ph
