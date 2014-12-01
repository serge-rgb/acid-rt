#include "vr.h"

#include "ph_gl.h"
#include "window.h"


#define PH_DEBUG_FRAMETIME

// Note 2: Workgroup size should be a multiple of workgroup size.
static int g_warpsize[] = {10, 16};
//static int g_warpsize[] = {1, 256};  // Nice trade-off between perf and artifacts.

namespace ph
{
namespace vr
{

static bool                      m_has_initted = false;

static GLuint                    m_quad_vao;
static GLuint                    m_quad_program;
static GLuint                    m_compute_program;

static float                     m_default_eye_z;       // Eye distance from plane.
static const OVR::HMDInfo*       m_hmdinfo;
static const OVR::HmdRenderInfo* m_renderinfo;
static float                     m_screen_size_m[2];    // Screen size in meters
static GLuint                    m_program = 0;
static GLuint                    m_postprocess_program = 0;
static GLuint                    m_screen_tex;
static GLuint                    m_backbuffer_tex;
static float                     m_lens_center_l[2];
static float                     m_lens_center_r[2];
static bool                      m_do_postprocessing = true;
static bool                      m_do_interlace_throttling = false;
static bool                      m_skybox_enabled = true;
static ovrEyeRenderDesc          m_render_desc_l;
static ovrEyeRenderDesc          m_render_desc_r;
static unsigned int              m_frame_index = 1;

ovrHmd                           m_hmd;

vr::HMDConsts                    m_cached_consts;

void init()
{
    // Safety net.
    if (m_has_initted)
    {
        phatal_error("vr::init called twice");
    }
    m_has_initted = true;


    if (!ovr_Initialize())
    {
        ph::phatal_error("Could not initialize OVR\n");
    }

    m_hmd = ovrHmd_Create(0);
    /*
     * create window here when direct mode works.
     */
    //ovrHmd_AttachToWindow(vr::m_hmd, glfwGetWin32Window(window::m_window), NULL, NULL);

    unsigned int sensor_caps =
    ovrTrackingCap_Position | ovrTrackingCap_Orientation | ovrTrackingCap_MagYawCorrection;

    ovrBool succ = ovrHmd_ConfigureTracking(m_hmd, sensor_caps, sensor_caps);
    if (!succ)
    {
        phatal_error("Could not initialize OVR sensors!");
    }

    auto fovPort_l = m_hmd->DefaultEyeFov[0];

    float theta = atanf(fovPort_l.DownTan);
    m_default_eye_z =  2 * abs(cosf(2 * theta));

    // Hard coded, taken from OVR source.
    m_screen_size_m[0] = 0.12576f;
    m_screen_size_m[1] = 0.07074f;

    const OVR::CAPI::HMDState* m_hmdstate = (OVR::CAPI::HMDState*)m_hmd->Handle;
    m_renderinfo                          = &m_hmdstate->RenderState.RenderInfo;
    m_hmdinfo                             = &m_hmdstate->RenderState.OurHMDInfo;


    m_lens_center_l[0] = (vr::m_screen_size_m[0] / 2) - (vr::m_renderinfo->LensSeparationInMeters / 2);
    m_lens_center_l[1] = vr::m_hmdinfo->CenterFromTopInMeters;

    m_lens_center_r[0] = vr::m_renderinfo->LensSeparationInMeters / 2;
    m_lens_center_r[1] = vr::m_hmdinfo->CenterFromTopInMeters;

    m_render_desc_l = ovrHmd_GetRenderDesc(m_hmd, ovrEye_Left, m_hmd->DefaultEyeFov[0]);
    m_render_desc_r = ovrHmd_GetRenderDesc(m_hmd, ovrEye_Right, m_hmd->DefaultEyeFov[1]);

    //////////////////////////////////////////////
    // Set up our cached constants
    //////////////////////////////////////////////

    memcpy(m_cached_consts.lens_centers[EYE_Left], m_lens_center_l, 2 * sizeof(float));
    memcpy(m_cached_consts.lens_centers[EYE_Right], m_lens_center_r, 2 * sizeof(float));

    m_cached_consts.eye_to_screen = m_default_eye_z;

    m_cached_consts.viewport_size_m[0] =
    m_screen_size_m[0] / 2;
    m_cached_consts.viewport_size_m[1] =
    m_screen_size_m[1];
    m_cached_consts.meters_per_tan_angle =
    m_renderinfo->EyeLeft.Distortion.MetersPerTanAngleAtCenter;
}

const HMDConsts get_hmd_constants()
{
    if (!m_has_initted)
    {
        phatal_error("Trying to get hmd info without initting module");
    }
    return m_cached_consts;
}

void fill_catmull_K (float* K, int num_coefficients)
{
    // auto lens_config = GenerateLensConfigFromEyeRelief(0.008f, *m_renderinfo);
    ph_assert(num_coefficients == OVR::LensConfig::NumCoefficients);

    for (int i = 0; i < num_coefficients; ++i)
    {
        K[i] = m_renderinfo->EyeLeft.Distortion.K[i];
    }
}

void toggle_postproc()
{
    m_do_postprocessing = m_do_postprocessing? false : true;
}

void toggle_interlace_throttle()
{
    m_do_interlace_throttling = m_do_interlace_throttling? false : true;
}

void enable_skybox()
{
    m_skybox_enabled = true;
    glUseProgram(m_program);
    glUniform1i(14, m_skybox_enabled);
}

void disable_skybox()
{
    m_skybox_enabled = false;
    glUseProgram(m_program);
    glUniform1i(14, m_skybox_enabled);
}

void begin_frame(Eye* left, Eye* right)
{
    ph_assert(left != NULL);
    ph_assert(right != NULL);
    ovrHmd_BeginFrameTiming(m_hmd, m_frame_index);

    ovrPosef poses[2];
    {
        ovrVector3f offsets[2] =
        {
            m_render_desc_l.HmdToEyeViewOffset,
            m_render_desc_r.HmdToEyeViewOffset
        };
        ovrHmd_GetEyePoses(m_hmd, 0, offsets, poses, /*ovrTrackingState*/NULL);
    }

    Eye* eyes[2] = { left, right };

    for (int i = 0; i < EYE_Count; ++i)
    {
        ovrPosef pose = poses[i];
        auto q = pose.Orientation;
        GLfloat quat[4]
        {
            q.x, q.y, q.z, q.w,
        };
        auto p = pose.Position;
        GLfloat camera_pos[3];
        io::get_wasd_camera(quat, camera_pos);
        // add pos
        camera_pos[0] += p.x;
        camera_pos[1] += p.y;
        camera_pos[2] += p.z;

        eyes[i]->position[0] = camera_pos[0];
        eyes[i]->position[1] = camera_pos[1];
        eyes[i]->position[2] = camera_pos[2];
        eyes[i]->orientation[0] = quat[0];
        eyes[i]->orientation[1] = quat[1];
        eyes[i]->orientation[2] = quat[2];
        eyes[i]->orientation[3] = quat[3];
    }
}

void end_frame()
{
    ovrHmd_EndFrameTiming(m_hmd);
    m_frame_index++;
}

void deinit()
{
    ovrHmd_Destroy(m_hmd);
    ovr_Shutdown();
}

}  // ns vr
}  // ns ph
