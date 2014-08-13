#pragma once

#include <ph.h>

namespace ph {
namespace vr {

extern float                     m_default_eye_z;     // Eye distance from plane.
extern ovrHmd                    m_hmd;
extern const OVR::HMDInfo*       m_hmdinfo;
extern const OVR::HmdRenderInfo* m_renderinfo;
extern float                     m_screen_size_m[2];  // Screen size in meters

void init();
void deinit();
}

namespace scene {

void init();

}
}
